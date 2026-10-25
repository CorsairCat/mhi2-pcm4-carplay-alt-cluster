import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.jar.*;
import org.objectweb.asm.*;
import org.objectweb.asm.tree.*;
import org.objectweb.asm.tree.analysis.*;

/** Offline addon generator for ONE converted MU1476 class. No JXE editing. */
public final class PatchPorsche implements Opcodes {
    static final String OWNER = "de/audi/tghu/fwhmi/porsche/DisplayManagerMIB2PorscheHigh";
    static final String BASE = "de/audi/tghu/fwhmi/DisplayManager";
    static final String BRIDGE = "local/pcm4/ProjectionBridge";
    static final String DESC = "(IILde/audi/atip/hmi/view/IDisplayListener;)V";
    static final String HASH = "551aab856826a16448c57a65705f5581f7e07aa3bab7823774351cbc56fbe0d3";
    static void require(boolean okay, String text) {
        if (!okay) throw new IllegalArgumentException(text);
    }
    static String digest(byte[] bytes) throws Exception {
        StringBuilder text = new StringBuilder();
        for (byte b : MessageDigest.getInstance("SHA-256").digest(bytes)) text.append(String.format("%02x", b & 255));
        return text.toString();
    }
    static AbstractInsnNode previous(AbstractInsnNode node) {
        do { node = node.getPrevious(); } while (node != null && node.getOpcode() < 0);
        return node;
    }
    static void variable(AbstractInsnNode node, int opcode, int slot) {
        require(node instanceof VarInsnNode && node.getOpcode() == opcode &&
                ((VarInsnNode)node).var == slot, "Unexpected superclass-call stack setup");
    }
    public static void main(String[] args) throws Exception {
        require(args.length == 3, "usage: PatchPorsche stock.jar bridge-classes NEW-addon.jar");
        Path destination = Paths.get(args[2]);
        require(!Files.exists(destination), "Refusing to replace an existing addon");
        byte[] input;
        try (JarFile jar = new JarFile(args[0])) {
            JarEntry entry = jar.getJarEntry(OWNER + ".class");
            require(entry != null, "Porsche class missing");
            try (InputStream stream = jar.getInputStream(entry); ByteArrayOutputStream out = new ByteArrayOutputStream()) {
                byte[] buffer = new byte[8192]; int n;
                while ((n = stream.read(buffer)) >= 0) out.write(buffer, 0, n);
                input = out.toByteArray();
            }
        }
        require(HASH.equals(digest(input)), "Converted Porsche class hash mismatch");
        ClassNode type = new ClassNode(); new ClassReader(input).accept(type, 0);
        require(OWNER.equals(type.name) && BASE.equals(type.superName), "Unexpected Porsche inheritance");
        type.fields.add(new FieldNode(ACC_PUBLIC|ACC_STATIC|ACC_FINAL,
            "PCM4_MU1476_PROJECTION_PATCH","I",null,1));
        MethodNode target = null;
        for (MethodNode method : type.methods) if (method.name.equals("switchContext") && method.desc.equals(DESC)) {
            require(target == null, "Duplicate switchContext"); target = method;
        }
        require(target != null && (target.access & ACC_SYNCHRONIZED) != 0, "Synchronized seam missing");
        MethodInsnNode call = null;
        for (AbstractInsnNode node = target.instructions.getFirst(); node != null; node = node.getNext()) {
            if (node instanceof MethodInsnNode) {
                MethodInsnNode candidate = (MethodInsnNode)node;
                if (candidate.getOpcode() == INVOKESPECIAL && candidate.owner.equals(BASE) &&
                    candidate.name.equals("switchContext") && candidate.desc.equals(DESC)) {
                    require(call == null, "Multiple superclass seams"); call = candidate;
                }
            }
        }
        require(call != null, "Superclass seam missing");
        AbstractInsnNode listener = previous(call), terminal = previous(listener),
                         context = previous(terminal), receiver = previous(context);
        variable(listener, ALOAD, 3); variable(terminal, ILOAD, 2);
        variable(context, ILOAD, 1); variable(receiver, ALOAD, 0);
        int raw = target.maxLocals++;
        InsnList entry = new InsnList();
        entry.add(new VarInsnNode(ILOAD, 1)); entry.add(new VarInsnNode(ISTORE, raw));
        target.instructions.insert(entry);
        InsnList hook = new InsnList();
        hook.add(new VarInsnNode(ALOAD, 0)); hook.add(new VarInsnNode(ILOAD, raw));
        hook.add(new VarInsnNode(ILOAD, 1)); hook.add(new VarInsnNode(ILOAD, 2));
        hook.add(new VarInsnNode(ALOAD, 3));
        hook.add(new MethodInsnNode(INVOKESTATIC, BRIDGE, "select",
            "(Lde/audi/atip/hmi/view/IDisplayManager;IIILde/audi/atip/hmi/view/IDisplayListener;)I", false));
        hook.add(new VarInsnNode(ISTORE, 1));
        hook.add(new VarInsnNode(ILOAD, 1)); hook.add(new VarInsnNode(ALOAD, 3));
        hook.add(new MethodInsnNode(INVOKESTATIC,BRIDGE,"listener",
            "(ILde/audi/atip/hmi/view/IDisplayListener;)Lde/audi/atip/hmi/view/IDisplayListener;",false));
        hook.add(new VarInsnNode(ASTORE,3));
        target.instructions.insertBefore(receiver, hook);
        MethodNode definitions = null;
        for (MethodNode method : type.methods)
            if (method.name.equals("defineContexts") && method.desc.equals("()V")) definitions=method;
        require(definitions != null, "Context declarations missing");
        int returns=0;
        for (AbstractInsnNode node : definitions.instructions.toArray()) if (node.getOpcode()==RETURN) {
            InsnList extension=new InsnList();
            extension.add(new VarInsnNode(ALOAD,0));
            extension.add(new VarInsnNode(ALOAD,0));
            extension.add(new FieldInsnNode(GETFIELD,BASE,"dc","[Lorg/dsi/ifc/displaymanagement/DisplayContext;"));
            extension.add(new MethodInsnNode(INVOKESTATIC,BRIDGE,"contexts",
                "([Lorg/dsi/ifc/displaymanagement/DisplayContext;)[Lorg/dsi/ifc/displaymanagement/DisplayContext;",false));
            extension.add(new FieldInsnNode(PUTFIELD,BASE,"dc","[Lorg/dsi/ifc/displaymanagement/DisplayContext;"));
            definitions.instructions.insertBefore(node,extension); returns++;
        }
        require(returns==1,"Expected exactly one defineContexts return");
        // Base getContext indexes the dense factory array by ID. Our private
        // context is appended for DSI declaration, so handle its public lookup
        // without allocating hundreds of null entries in that declaration list.
        for (MethodNode method : type.methods)
            require(!(method.name.equals("getDisplayables") && method.desc.equals("(I)[I")),
                    "Unexpected Porsche displayable lookup override");
        MethodNode lookup=new MethodNode(ACC_PUBLIC,"getDisplayables","(I)[I",null,null);
        LabelNode factory=new LabelNode();
        lookup.instructions.add(new VarInsnNode(ILOAD,1));
        lookup.instructions.add(new MethodInsnNode(INVOKESTATIC,BRIDGE,"projectionDisplayables","(I)[I",false));
        lookup.instructions.add(new InsnNode(DUP));
        lookup.instructions.add(new JumpInsnNode(IFNULL,factory));
        lookup.instructions.add(new InsnNode(ARETURN));
        lookup.instructions.add(factory);lookup.instructions.add(new InsnNode(POP));
        lookup.instructions.add(new VarInsnNode(ALOAD,0));lookup.instructions.add(new VarInsnNode(ILOAD,1));
        lookup.instructions.add(new MethodInsnNode(INVOKESPECIAL,BASE,"getDisplayables","(I)[I",false));
        lookup.instructions.add(new InsnNode(ARETURN));lookup.maxLocals=2;type.methods.add(lookup);
        // jxe2jar promotes classes with synthetic accessors to v49. J9's
        // foundation runtime needs v48; ASM represents Synthetic as its legacy
        // attribute. Reject actual Java5+ features before normalizing the version.
        require(type.signature==null && (type.access&(ACC_ENUM|ACC_ANNOTATION))==0,
                "Unexpected Java5 class feature");
        require(type.visibleAnnotations==null && type.invisibleAnnotations==null,
                "Unexpected class annotations");
        for(FieldNode field:type.fields) require(field.signature==null && (field.access&ACC_ENUM)==0,
                "Unexpected Java5 field feature");
        for(MethodNode method:type.methods) {
            require(method.signature==null && (method.access&(ACC_BRIDGE|ACC_VARARGS))==0,
                    "Unexpected Java5 method feature");
            require(method.visibleAnnotations==null && method.invisibleAnnotations==null,
                    "Unexpected method annotations");
            for(AbstractInsnNode node:method.instructions.toArray())
                require(node.getOpcode()!=INVOKEDYNAMIC,"Unexpected invokedynamic");
        }
        type.version=V1_4;
        // Preserve original branches and recompute stack sizes without loading OEM classes.
        ClassWriter writer = new ClassWriter(ClassWriter.COMPUTE_MAXS); type.accept(writer);
        byte[] patched = writer.toByteArray();
        ClassNode verified = new ClassNode(); new ClassReader(patched).accept(verified, 0);
        for (MethodNode method : verified.methods) if ((method.name.equals("switchContext") && method.desc.equals(DESC)) || method.name.equals("defineContexts") || method.name.equals("getDisplayables"))
            new Analyzer<BasicValue>(new BasicVerifier()).analyze(verified.name, method);
        Path classes = Paths.get(args[1]);
        require(Files.isRegularFile(classes.resolve(BRIDGE + ".class")), "Bridge class missing");
        List<Path> bridgeFiles = new ArrayList<Path>();
        try (java.util.stream.Stream<Path> stream = Files.walk(classes)) {
            stream.filter(p -> Files.isRegularFile(p) && p.toString().endsWith(".class")).forEach(bridgeFiles::add);
        }
        Collections.sort(bridgeFiles);
        try (JarOutputStream jar = new JarOutputStream(Files.newOutputStream(destination, StandardOpenOption.CREATE_NEW))) {
            jar.putNextEntry(new JarEntry(OWNER + ".class")); jar.write(patched); jar.closeEntry();
            for (Path file : bridgeFiles) {
                String name = classes.relativize(file).toString().replace(File.separatorChar, '/');
                require((name.startsWith("local/pcm4/ProjectionBridge") || name.equals("local/pcm4/LoadProbe.class")), "Unexpected addon class: " + name);
                jar.putNextEntry(new JarEntry(name)); jar.write(Files.readAllBytes(file)); jar.closeEntry();
            }
        }
        System.out.println("PASS: exact input hash, one post-KDK seam, preserved listener, stack/local analysis");
        System.out.println("Patched class SHA256 " + digest(patched));
        System.out.println("Addon only; no loader configuration or installation performed.");
    }
}

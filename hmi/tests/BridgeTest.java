import local.pcm4.ProjectionBridge;
import de.audi.atip.hmi.view.IDisplayManager;
import de.audi.atip.hmi.view.IDisplayListener;
import java.io.*;
import java.lang.reflect.*;

/** JVM tests against the actual converted Porsche interface, not a fake ABI. */
public final class BridgeTest {
    static volatile int current = 72, lastRaw = -1, calls;
    static volatile IDisplayListener observed;
    static final IDisplayListener listener = new IDisplayListener() {
        public void activeContext(int c, int t) { }
    };
    static void require(boolean okay, String text) { if (!okay) throw new AssertionError(text); }
    static void await(int expected, String text) throws Exception {
        long until = System.currentTimeMillis() + 2500;
        while (current != expected && System.currentTimeMillis() < until) Thread.sleep(20);
        require(current == expected, text + ": current=" + current);
    }
    static void marker(File file, String value) throws Exception {
        File temp = new File(file.getPath() + ".next");
        try (FileOutputStream stream = new FileOutputStream(temp)) { stream.write(value.getBytes("US-ASCII")); }
        require(temp.renameTo(file), "marker rename");
    }
    public static void main(String[] args) throws Exception {
        File directory = new File(args[0]); directory.mkdirs();
        File ready = new File(directory, "ready"), disable = new File(directory, "disable");
        System.setProperty("pcm4.cluster.context", "900"); // SYNTHETIC TEST PROFILE ONLY
        System.setProperty("pcm4.cluster.marker", ready.getAbsolutePath());
        System.setProperty("pcm4.cluster.disable", disable.getAbsolutePath());
        final IDisplayManager[] manager = new IDisplayManager[1];
        manager[0] = (IDisplayManager)Proxy.newProxyInstance(BridgeTest.class.getClassLoader(),
            new Class[] {IDisplayManager.class}, new InvocationHandler() {
                public Object invoke(Object proxy, Method method, Object[] values) {
                    if (method.getName().equals("getCurrentContextID")) return Integer.valueOf(current);
                    if (method.getName().equals("switchContext")) {
                        synchronized (proxy) {
                            int raw = ((Integer)values[0]).intValue();
                            int terminal = ((Integer)values[1]).intValue();
                            // Deliberately non-idempotent remap catches replaying
                            // a mapped context instead of the original request.
                            int mapped = raw == 5 ? 72 : raw == 72 ? 151 : raw;
                            lastRaw = raw; observed = (IDisplayListener)values[2]; calls++;
                            current = ProjectionBridge.select(manager[0],raw,mapped,terminal,observed);
                        }
                    }
                    return null;
                }
            });
        manager[0].switchContext(5,1,listener);
        require(current == 72, "no takeover without heartbeat");
        marker(ready,"PCM4READY1\n1\n1\n"); await(900,"automatic takeover");
        require(lastRaw == 5 && observed == listener, "raw request/listener preserved");
        marker(ready,"PCM4READY1\n1\n1\ntrailing"); await(72,"invalid marker restores");
        require(lastRaw == 5, "must not remap 72 into 151");
        marker(ready,"PCM4READY1\n1\n2\n"); await(900,"reacquire");
        manager[0].switchContext(12,1,listener); await(12,"other page wins");
        Thread.sleep(250); require(current == 12, "watchdog must not steal other page");
        manager[0].switchContext(5,1,listener); await(900,"nav page reacquires");
        try (FileOutputStream stream = new FileOutputStream(disable)) { stream.write(1); }
        await(72,"disable restores"); require(disable.delete(),"delete disable");
        marker(ready,"PCM4READY1\n1\n3\n"); await(900,"reacquire before expiry");
        require(ready.setLastModified(System.currentTimeMillis()-5000),"age heartbeat");
        await(72,"stale heartbeat restores");
        require(ProjectionBridge.select(manager[0],33,33,0,listener) == 33,"other terminal untouched");
        require(calls >= 7,"expected actual bridge dispatches");
        // A genuine stock use of the configured candidate must win, including
        // subsequent watchdog ticks, rather than being 'restored' away.
        marker(ready,"PCM4READY1\n1\n4\n"); await(900,"ready before collision");
        manager[0].switchContext(900,1,listener);
        Thread.sleep(300); require(current == 900,"stock allocation collision must stop ownership");
        Class<?> patched = Class.forName("de.audi.tghu.fwhmi.porsche.DisplayManagerMIB2PorscheHigh", false,
                                         BridgeTest.class.getClassLoader());
        patched.getDeclaredMethods();
        System.out.println("PASS: real Porsche interface, raw/KDK replay, listener, automatic takeover/restore, page priority, heartbeat/disable");
        System.out.println("PASS: patched Porsche class links under JVM verification without initializing OEM HMI");
    }
}

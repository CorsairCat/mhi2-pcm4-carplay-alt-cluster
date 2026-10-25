import local.pcm4.ProjectionBridge;
import org.dsi.ifc.displaymanagement.DisplayContext;
public final class ContextCollisionTest {
    public static void main(String[] args) {
        System.setProperty("pcm4.cluster.context","900");
        DisplayContext[] factory=new DisplayContext[]{new DisplayContext(900,new int[]{33})};
        if(ProjectionBridge.contexts(factory)!=factory)
            throw new AssertionError("Must preserve conflicting factory declaration");
        if(ProjectionBridge.select(null,72,72,1,null)!=72)
            throw new AssertionError("Declaration collision must disable takeover");
        System.out.println("PASS: declaration collision preserves stock and disables takeover");
    }
}

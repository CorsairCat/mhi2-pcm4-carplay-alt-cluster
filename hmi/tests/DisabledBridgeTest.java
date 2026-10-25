import local.pcm4.ProjectionBridge;
public final class DisabledBridgeTest {
    public static void main(String[] args) {
        // A separate JVM, with no pcm4.cluster.context property. No target
        // display-manager instance is required or touched in disabled mode.
        if (ProjectionBridge.select(null,72,72,1,null) != 72)
            throw new AssertionError("Default configuration must pass stock context through");
        System.out.println("PASS: bridge disabled by default");
    }
}

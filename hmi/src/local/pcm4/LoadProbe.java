package local.pcm4;

/** Read-only class-loader smoke test. Does not initialize the OEM HMI class. */
public final class LoadProbe {
    public static void main(String[] args) throws Exception {
        Class type=Class.forName("de.audi.tghu.fwhmi.porsche.DisplayManagerMIB2PorscheHigh",
                                 false,ProjectionBridge.class.getClassLoader());
        type.getDeclaredField("PCM4_MU1476_PROJECTION_PATCH");
        type.getDeclaredMethods();
        System.out.println("PASS: patched Porsche class selected and linked; OEM initialization not executed");
    }
}

package local.pcm4;

import java.io.File;
import java.io.FileInputStream;
import de.audi.atip.hmi.view.IDisplayManager;
import de.audi.atip.hmi.view.IDisplayListener;

/** Original Porsche bridge. Disabled unless a reviewed context is configured.
 * Hook runs AFTER KDK mapping but also receives the saved RAW entry context.
 * No disk I/O or display-manager calls occur on the intercepted hook path.
 * This class does not declare contexts, choose a layer or change nav metadata.
 */
public final class ProjectionBridge implements Runnable {
    private static final int TERMINAL = 1;
    private static final long TTL = 3000;
    private static final int CONTEXT = contextProperty();
    private static final File MARKER = new File(System.getProperty(
        "pcm4.cluster.marker", "/tmp/pcm4-cluster-ready"));
    private static final File DISABLE = new File(System.getProperty(
        "pcm4.cluster.disable", "/tmp/pcm4-cluster-disable"));
    private static final ThreadLocal INTERNAL = new ThreadLocal();
    private static volatile Request latest;
    private static volatile boolean ready;
    private static Thread worker;
    private static volatile boolean stopped;

    private static final class Request {
        final IDisplayManager manager;
        final int raw, mapped;
        final IDisplayListener listener;
        Request(IDisplayManager manager, int raw, int mapped, IDisplayListener listener) {
            this.manager = manager; this.raw = raw; this.mapped = mapped; this.listener = listener;
        }
    }
    private ProjectionBridge() { }
    private static int contextProperty() {
        try {
            int context = Integer.parseInt(System.getProperty("pcm4.cluster.context", "-1"));
            // Profile allocation is external. Reject common stock range outright.
            return context > 255 && context <= 65535 ? context : -1;
        } catch (Throwable ignored) { return -1; }
    }
    private static boolean navigation(int context) { return context >= 72 && context <= 77; }
    private static synchronized void startWorker() {
        if (worker != null || CONTEXT < 0 || stopped) return;
        worker = new Thread(new ProjectionBridge(), "PCM4-projection-watchdog");
        worker.setDaemon(true);
        try { worker.start(); }
        catch (Throwable ignored) { worker = null; ready = false; }
    }
    /** Called under the synchronized Porsche display-manager method. */
    public static int select(IDisplayManager dm, int raw, int mapped,
                             int terminal, IDisplayListener listener) {
        if (terminal != TERMINAL || CONTEXT < 0) return mapped;
        Integer forced = (Integer) INTERNAL.get();
        if (forced != null) return forced.intValue() < 0 ? mapped : forced.intValue();
        // A stock request using our projection context signals an allocation
        // collision. Stop claiming it; retain the prior actual stock request.
        if (mapped == CONTEXT) { ready = false; stopped = true; latest = null; return mapped; }
        latest = new Request(dm, raw, mapped, listener);
        startWorker();
        return ready && navigation(mapped) ? CONTEXT : mapped;
    }
    private static boolean freshMarker() {
        if (DISABLE.exists()) return false;
        long stamp = MARKER.lastModified(), now = System.currentTimeMillis();
        if (stamp <= 0 || now < stamp || now - stamp >= TTL) return false;
        FileInputStream stream = null;
        try {
            stream = new FileInputStream(MARKER);
            byte[] bytes = new byte[96];
            int count = 0, n;
            while (count < bytes.length && (n = stream.read(bytes, count, bytes.length-count)) > 0) count += n;
            if (stream.read() != -1) return false;
            String value = new String(bytes, 0, count, "US-ASCII");
            if (!value.startsWith("PCM4READY1\n") || !value.endsWith("\n")) return false;
            int split = value.indexOf('\n', 11);
            if (split < 0 || split == value.length()-1) return false;
            String session = value.substring(11, split);
            String frames = value.substring(split+1, value.length()-1);
            if (!decimal(session) || !decimal(frames)) return false;
            return Long.parseLong(session) > 0 && Long.parseLong(frames) > 0;
        } catch (Throwable ignored) { return false; }
        finally { if (stream != null) try { stream.close(); } catch (Throwable ignored) { } }
    }
    private static boolean decimal(String text) {
        if (text.length() == 0 || text.length() > 19) return false;
        for (int i=0; i<text.length(); i++) if (text.charAt(i) < '0' || text.charAt(i) > '9') return false;
        return true;
    }
    private static void reconcile() {
        Request request = latest;
        if (request == null) return;
        // Same monitor as the stock synchronized switchContext method. Re-read
        // the request under it so a newer stock page cannot be overwritten.
        synchronized (request.manager) {
            if (request != latest) return;
            int current = request.manager.getCurrentContextID(TERMINAL);
            boolean take = ready && navigation(request.mapped);
            if (take && current == request.mapped) {
                INTERNAL.set(new Integer(CONTEXT));
            } else if (!take && current == CONTEXT) {
                INTERNAL.set(new Integer(-1));
            } else return;
            try {
                // Re-enter using RAW context: Porsche applies KDK exactly once.
                request.manager.switchContext(request.raw, TERMINAL, request.listener);
            } finally { INTERNAL.set(null); }
        }
    }
    public void run() {
        for (;;) {
            try {
                ready = !stopped && freshMarker();
                reconcile();
                Thread.sleep(200);
            } catch (InterruptedException ignored) {
                stopped = true; ready = false;
            } catch (Throwable ignored) {
                stopped = true; ready = false;
                try { Thread.sleep(200); } catch (InterruptedException stop) { stopped = true; }
            }
        }
    }
}

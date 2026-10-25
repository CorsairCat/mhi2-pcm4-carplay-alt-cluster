#include "pcm4_handover.h"
#include <string.h>

int pcm4_handover_init(struct pcm4_handover *h, const struct pcm4_handover_ops *ops, uint64_t timeout)
{
    if (!h || !ops || !ops->take || !ops->visible || !ops->restore || !timeout) return -1;
    memset(h, 0, sizeof(*h)); h->ops = *ops; h->timeout_ms = timeout;
    return 0;
}
static int release(struct pcm4_handover *h)
{
    int hide, restore;
    if (!h->owned) return 0;
    hide = h->ops.visible(h->ops.opaque, 0);
    restore = h->ops.restore(h->ops.opaque, h->stock_context);
    /* Retry on later ticks if either stage failed. Never reacquire on error. */
    if (hide || restore) { h->faulted = 1; return -1; }
    h->owned = 0; return 0;
}
int pcm4_handover_tick(struct pcm4_handover *h, uint64_t now)
{
    int desired;
    if (!h || !h->ops.take) return -1;
    if (h->have_clock && now < h->last_now_ms) { h->ready = 0; h->faulted = 1; }
    h->have_clock = 1; h->last_now_ms = now;
    if (h->ready && (now < h->last_frame_ms || now - h->last_frame_ms >= h->timeout_ms)) h->ready = 0;
    desired = h->connected && h->ready && !h->faulted && h->have_stock &&
              h->stock_context >= 72 && h->stock_context <= 77;
    if (!desired) return release(h);
    if (!h->owned) {
        /* take() must be atomic: nonzero means it acquired nothing. */
        if (h->ops.take(h->ops.opaque)) { h->faulted = 1; return -1; }
        h->owned = 1;
        if (h->ops.visible(h->ops.opaque, 1)) {
            h->faulted = 1; release(h); return -1;
        }
    }
    return 0;
}
int pcm4_handover_stock(struct pcm4_handover *h, unsigned terminal, uint32_t context, uint64_t now)
{
    if (!h) return -1;
    if (terminal != 1) return pcm4_handover_tick(h, now);
    h->stock_context = context; h->have_stock = 1;
    return pcm4_handover_tick(h, now);
}
int pcm4_handover_connected(struct pcm4_handover *h, int connected, uint64_t now)
{
    if (!h || (connected != 0 && connected != 1)) return -1;
    if (!connected) { h->connected = 0; h->ready = 0; }
    else if (!h->connected) {
        /* Clear a prior fault only after all prior ownership was released. */
        if (h->owned && release(h)) return -1;
        h->connected = 1; h->ready = 0; h->faulted = 0;
    }
    return pcm4_handover_tick(h, now);
}
int pcm4_handover_frame(struct pcm4_handover *h, uint64_t now)
{
    if (!h || !h->connected) return -1;
    h->last_frame_ms = now; h->ready = 1;
    return pcm4_handover_tick(h, now);
}

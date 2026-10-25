#ifndef PCM4_HANDOVER_H
#define PCM4_HANDOVER_H
#include <stdint.h>

/* Policy core only. Target adapter must preserve full Java listener/request
 * state, execute callbacks on the proper HMI thread, and tick even after a
 * renderer process dies. No HMI context/layer is chosen by this module.
 */
struct pcm4_handover_ops {
    int (*take)(void *);
    int (*visible)(void *, int);
    int (*restore)(void *, uint32_t latest_stock_context);
    void *opaque;
};
struct pcm4_handover {
    struct pcm4_handover_ops ops;
    uint64_t last_frame_ms, timeout_ms, last_now_ms;
    uint32_t stock_context;
    unsigned have_stock, connected, ready, owned, faulted, have_clock;
};
int pcm4_handover_init(struct pcm4_handover *, const struct pcm4_handover_ops *, uint64_t);
/* Call for the POST Porsche KDK-remap request. Other terminals ignored. */
int pcm4_handover_stock(struct pcm4_handover *, unsigned terminal, uint32_t, uint64_t now_ms);
int pcm4_handover_connected(struct pcm4_handover *, int, uint64_t now_ms);
/* Only after a complete AU was accepted by the decoder. */
int pcm4_handover_frame(struct pcm4_handover *, uint64_t now_ms);
int pcm4_handover_tick(struct pcm4_handover *, uint64_t now_ms);
#endif

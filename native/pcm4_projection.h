#ifndef PCM4_PROJECTION_H
#define PCM4_PROJECTION_H
#include "pcm4_aux_stream.h"
#include "pcm4_handover.h"
#include "pcm4_mu1476_display.h"

/* In-process integration seam, caller-owned open display + serialized events.
 * Fresh zero-initialized storage only. No transport, crypto or Java hook here.
 */
struct pcm4_projection {
    struct pcm4_aux_stream stream;
    struct pcm4_handover handover;
    struct pcm4_display *display;
    int (*take)(void *);
    int (*restore)(void *, uint32_t);
    void *hmi_opaque;
    uint64_t now_ms, accepted_frames, last_frame_ms, timeout_ms;
    int (*publish)(void *, uint64_t, uint64_t);
    void *publisher;
};
int pcm4_projection_init(struct pcm4_projection *, struct pcm4_display *,
                         int (*take)(void *), int (*restore)(void *, uint32_t),
                         void *hmi_opaque, uint64_t frame_timeout_ms);
/* External Java owner: native code only shows video and publishes readiness.
 * No stock-context callbacks or native acquisition policy run in this mode. */
int pcm4_projection_init_external(struct pcm4_projection *, struct pcm4_display *,
                                  int (*publish)(void *, uint64_t, uint64_t),
                                  void *publisher, uint64_t timeout_ms);
int pcm4_projection_tick(struct pcm4_projection *, uint64_t now_ms);
int pcm4_projection_begin(struct pcm4_projection *, uint64_t generation, uint64_t now_ms);
int pcm4_projection_configure(struct pcm4_projection *, uint64_t, const void *, size_t, uint64_t now_ms);
int pcm4_projection_feed(struct pcm4_projection *, uint64_t, const void *, size_t,
                         uint64_t vendor_timestamp, uint64_t now_ms, void *scratch, size_t);
int pcm4_projection_end(struct pcm4_projection *, uint64_t, uint64_t now_ms);
/* Feed post-remap stock requests and independent periodic ticks to p.handover.
 * Shut down/release successfully before closing the caller-owned display.
 */
#endif

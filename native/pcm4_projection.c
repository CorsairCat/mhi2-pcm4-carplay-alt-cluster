#include "pcm4_projection.h"
#include <string.h>
static int take(void *opaque)
{
    struct pcm4_projection *p = opaque;
    return p->take(p->hmi_opaque);
}
static int restore(void *opaque, uint32_t context)
{
    struct pcm4_projection *p = opaque;
    return p->restore(p->hmi_opaque, context);
}
static int visible(void *opaque, int show)
{
    struct pcm4_projection *p = opaque;
    return pcm4_display_set_visible(p->display, show);
}
static int emit(void *opaque, const void *data, size_t length, uint64_t timestamp)
{
    struct pcm4_projection *p = opaque;
    if (pcm4_display_decode(p->display, data, length, timestamp)) return -1;
    return pcm4_handover_frame(&p->handover, p->now_ms);
}
int pcm4_projection_init(struct pcm4_projection *p, struct pcm4_display *display,
                         int (*take_hmi)(void *), int (*restore_hmi)(void *, uint32_t),
                         void *opaque, uint64_t timeout)
{
    struct pcm4_handover_ops ops;
    if (!p || !display || !display->handle || !take_hmi || !restore_hmi || !timeout) return -1;
    memset(p, 0, sizeof(*p)); p->display = display;
    p->take = take_hmi; p->restore = restore_hmi; p->hmi_opaque = opaque;
    ops.take = take; ops.restore = restore; ops.visible = visible; ops.opaque = p;
    return pcm4_handover_init(&p->handover, &ops, timeout);
}
int pcm4_projection_begin(struct pcm4_projection *p, uint64_t generation, uint64_t now)
{
    if (!p || !p->display || !generation || generation <= p->stream.generation) return -1;
    if (pcm4_handover_connected(&p->handover, 0, now)) return -1;
    if (pcm4_aux_begin(&p->stream, generation, emit, p)) return -1;
    return pcm4_handover_connected(&p->handover, 1, now);
}
int pcm4_projection_end(struct pcm4_projection *p, uint64_t generation, uint64_t now)
{
    if (!p || generation != p->stream.generation) return -1;
    pcm4_aux_end(&p->stream, generation);
    return pcm4_handover_connected(&p->handover, 0, now);
}
int pcm4_projection_configure(struct pcm4_projection *p, uint64_t generation,
                              const void *data, size_t length, uint64_t now)
{
    int rc;
    if (!p || generation != p->stream.generation) return -1;
    /* Reconfiguration drops ownership until a new IDR has decoded. */
    if (pcm4_handover_connected(&p->handover, 0, now)) return -1;
    rc = pcm4_aux_configure(&p->stream, generation, data, length);
    if (rc) { pcm4_aux_end(&p->stream, generation); return rc; }
    return pcm4_handover_connected(&p->handover, 1, now);
}
int pcm4_projection_feed(struct pcm4_projection *p, uint64_t generation,
                         const void *data, size_t length, uint64_t timestamp,
                         uint64_t now, void *scratch, size_t capacity)
{
    int rc;
    if (!p || generation != p->stream.generation) return -1;
    p->now_ms = now;
    rc = pcm4_aux_access_unit(&p->stream, generation, data, length, timestamp, scratch, capacity);
    if (rc < 0) pcm4_projection_end(p, generation, now);
    return rc;
}

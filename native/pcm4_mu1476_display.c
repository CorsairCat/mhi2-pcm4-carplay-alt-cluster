#include "pcm4_mu1476_display.h"
#include <string.h>

static void word(unsigned char *p, unsigned at, uint32_t value)
{
    /* Explicit little endian, matching the inspected ARM ELF. */
    p[at] = (unsigned char)value;
    p[at+1] = (unsigned char)(value >> 8);
    p[at+2] = (unsigned char)(value >> 16);
    p[at+3] = (unsigned char)(value >> 24);
}

int pcm4_display_close(struct pcm4_display *d)
{
    int rc = 0;
    if (!d) return -1;
    if (d->handle) rc = d->api->close(d->handle);
    d->handle = NULL;
    d->api = NULL;
    return rc;
}

int pcm4_display_open(struct pcm4_display *d, const struct pcm4_nv_api *a,
                      const struct pcm4_display_config *c)
{
    struct pcm4_nv_open p;
    unsigned char attrs[64];
    int rc;
    if (!d || d->api || d->handle || !a || !c ||
        !a->open || !a->configure ||
        !a->get_attributes || !a->set_attributes || !a->decode || !a->close)
        return -1;
    if (c->output_device > 1 || !c->output_width || !c->output_height ||
        !c->video.width || !c->video.height ||
        !(c->video.fps > 0.0f && c->video.fps <= 60.0f) ||
        !c->destination.width || !c->destination.height ||
        c->destination.x >= c->output_width || c->destination.y >= c->output_height ||
        c->destination.width > c->output_width - c->destination.x ||
        c->destination.height > c->output_height - c->destination.y)
        return -1;
    memset(&p, 0, sizeof(p));
    p.output_device = c->output_device;
    p.priority = c->priority;
    p.use_layer = 1;
    p.layer = c->layer;
    /* Offset 16 must be zero. Audi's value 8 is rejected by this binary. */
    p.stream_type = 0;
    d->api = a;
    /* NvSS owns kdInitializeNV/kdTerminateNV internally. This libKD's
     * repeated initialize is a no-op, not a separately owned reference. */
    rc = a->open(&d->handle, &p);
    if (rc || !d->handle) { if (!rc) rc = -1; goto fail; }
    rc = a->configure(d->handle, &c->video);
    if (rc) goto fail;
    memset(attrs, 0, sizeof(attrs));
    rc = a->get_attributes(d->handle, attrs);
    if (rc) goto fail;
    attrs[8] = 1;
    word(attrs, 12, 0); word(attrs, 16, 0);
    word(attrs, 20, c->video.width); word(attrs, 24, c->video.height);
    word(attrs, 28, c->destination.x); word(attrs, 32, c->destination.y);
    word(attrs, 36, c->destination.width); word(attrs, 40, c->destination.height);
    /* Update geometry only. Color/visibility command flags stay zero. */
    attrs[44] = 0;
    attrs[60] = 0;
    rc = a->set_attributes(d->handle, attrs);
    if (!rc) return 0;
fail:
    pcm4_display_close(d);
    return rc;
}

int pcm4_display_decode(struct pcm4_display *d, const void *data, size_t size,
                        uint64_t timestamp)
{
    struct pcm4_nv_input in;
    if (!d || !d->handle || !data || !size || size > 512000) return -1;
    memset(&in, 0, sizeof(in));
    in.data = data;
    in.length = (uint32_t)size;
    in.timestamp = timestamp;
    /* Caller supplies one complete Annex-B access unit, including parameter
     * sets at the appropriate stream boundary. No raw TCP chunk ingestion. */
    return d->api->decode(d->handle, &in, 0);
}

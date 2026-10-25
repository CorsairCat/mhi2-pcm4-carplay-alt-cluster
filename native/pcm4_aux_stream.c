#include "pcm4_aux_stream.h"
#include <string.h>

static int parameter(struct pcm4_aux_stream *s, const unsigned char *p,
                     size_t length, size_t *at, unsigned type)
{
    size_t count;
    if (*at > length || length - *at < 2) return -1;
    count = (size_t)p[*at] << 8 | p[*at + 1]; *at += 2;
    if (!count || count > length - *at || (p[*at] & 0x80) ||
        (p[*at] & 31) != type || count + 4 > sizeof(s->parameters) - s->parameters_size) return -1;
    memcpy(s->parameters + s->parameters_size, "\0\0\0\1", 4);
    memcpy(s->parameters + s->parameters_size + 4, p + *at, count);
    s->parameters_size += count + 4; *at += count;
    return 0;
}
int pcm4_aux_begin(struct pcm4_aux_stream *s, uint64_t generation,
                   int (*emit)(void *, const void *, size_t, uint64_t), void *opaque)
{
    if (!s || !emit || !generation || generation <= s->generation) return -1;
    memset(s, 0, sizeof(*s));
    s->generation = generation; s->emit = emit; s->opaque = opaque;
    return 0;
}
void pcm4_aux_end(struct pcm4_aux_stream *s, uint64_t generation)
{
    if (!s || s->generation != generation) return;
    /* Keep generation watermark: callbacks from the ended session stay stale. */
    s->configured = s->primed = 0; s->parameters_size = 0; s->emit = NULL;
}
int pcm4_aux_configure(struct pcm4_aux_stream *s, uint64_t generation,
                       const void *data, size_t length)
{
    const unsigned char *p = data;
    size_t at = 6;
    unsigned i, count;
    if (!s || !s->emit || s->generation != generation) return -1;
    /* A failed reconfiguration invalidates old parameter sets as well. */
    s->configured = s->primed = 0; s->parameters_size = 0;
    if (!p || length < 7 || p[0] != 1 || (p[4] & 0xfc) != 0xfc ||
        (p[5] & 0xe0) != 0xe0 || (p[4] & 3) == 2) return -1;
    s->nal_length_size = (p[4] & 3) + 1;
    count = p[5] & 31;
    if (!count) return -1;
    for (i = 0; i < count; ++i) if (parameter(s, p, length, &at, 7)) return -1;
    if (at >= length || !(count = p[at++])) return -1;
    for (i = 0; i < count; ++i) if (parameter(s, p, length, &at, 8)) return -1;
    /* Optional high-profile extension syntax is not implemented: reject it
     * rather than interpreting unknown trailing bytes as valid config. */
    if (at != length) return -1;
    s->configured = 1; return 0;
}
int pcm4_aux_access_unit(struct pcm4_aux_stream *s, uint64_t generation,
                         const void *data, size_t length, uint64_t timestamp,
                         void *scratch, size_t capacity)
{
    const unsigned char *p = data;
    unsigned char *out = scratch;
    size_t at = 0, written = 0, count, i;
    unsigned idr = 0, vcl = 0;
    if (!s || !s->emit || s->generation != generation || !p || !out ||
        !length || length > 512000 || capacity < 512000) return -1;
    if (!s->configured) return 1;
    /* Two passes: validate the entire AU before publishing any bytes. */
    while (at < length) {
        if (length - at < s->nal_length_size) goto malformed;
        count = 0;
        for (i = 0; i < s->nal_length_size; ++i) count = (count << 8) | p[at++];
        if (!count || count > length - at || (p[at] & 0x80) || !(p[at] & 31) || (p[at] & 31) > 23) goto malformed;
        if ((p[at] & 31) == 5) idr = 1;
        if ((p[at] & 31) >= 1 && (p[at] & 31) <= 5) vcl = 1;
        if (count + 4 > 512000 - written) goto malformed;
        written += count + 4; at += count;
    }
    if (!vcl || (!s->primed && !idr)) return 1;
    if (idr && s->parameters_size > 512000 - written) goto malformed;
    written = 0;
    if (idr) {
        memcpy(out, s->parameters, s->parameters_size); written = s->parameters_size;
    }
    at = 0;
    while (at < length) {
        count = 0;
        for (i = 0; i < s->nal_length_size; ++i) count = (count << 8) | p[at++];
        memcpy(out + written, "\0\0\0\1", 4);
        memcpy(out + written + 4, p + at, count);
        written += count + 4; at += count;
    }
    if (s->emit(s->opaque, out, written, timestamp)) goto malformed;
    s->primed = 1; return 0;
malformed:
    s->primed = 0; /* Resume only at a fresh IDR after loss/decode failure. */
    return -1;
}

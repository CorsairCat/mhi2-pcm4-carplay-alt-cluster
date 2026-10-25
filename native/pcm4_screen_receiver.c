#include "pcm4_screen_receiver.h"
#include <string.h>

int pcm4_screen_receiver_init(struct pcm4_screen_receiver *r, struct pcm4_projection *p,
                              uint64_t generation, void *body, size_t body_capacity,
                              void *scratch, size_t scratch_capacity,
                              int (*decrypt)(void *, void *, size_t), void *crypto)
{
    if (!r || !p || !generation || p->stream.generation != generation ||
        !p->stream.emit || !body || !scratch || body == scratch ||
        body_capacity < 512000 || scratch_capacity < 512000 || !decrypt) return -1;
    memset(r, 0, sizeof(*r));
    r->projection = p; r->generation = generation;
    r->body = body; r->body_capacity = body_capacity;
    r->scratch = scratch; r->scratch_capacity = scratch_capacity;
    r->decrypt_frame = decrypt; r->crypto = crypto;
    return 0;
}
static int fail(struct pcm4_screen_receiver *r, uint64_t now)
{
    r->failed = 1;
    pcm4_projection_end(r->projection, r->generation, now);
    return -1;
}
static int message(struct pcm4_screen_receiver *r, uint64_t now)
{
    int rc;
    switch (r->header[4]) {
    case 0:
        if (!r->body_size || r->decrypt_frame(r->crypto, r->body, r->body_size)) return fail(r, now);
        /* Vendor timestamp units are unresolved; use zero as controlled tests
         * do. Do not reinterpret the Screen header's timestamp as NvSS units. */
        rc = pcm4_projection_feed(r->projection, r->generation, r->body,
                                 r->body_size, 0, now, r->scratch, r->scratch_capacity);
        if (rc < 0) return fail(r, now);
        if (!rc) r->accepted_frames++;
        break;
    case 1:
        if (!r->body_size || pcm4_projection_configure(r->projection, r->generation,
                                                      r->body, r->body_size, now)) return fail(r, now);
        break;
    default:
        /* Known control/keepalive opcodes 2..5 carry no decoder readiness. */
        break;
    }
    r->messages++;
    r->header_used = r->body_used = r->body_size = 0;
    return 0;
}
int pcm4_screen_receiver_feed(struct pcm4_screen_receiver *r, uint64_t generation,
                              const void *data, size_t length, uint64_t now)
{
    const unsigned char *p = data;
    size_t amount;
    if (!r || !r->projection || r->failed || r->generation != generation ||
        r->projection->stream.generation != generation || !r->projection->stream.emit ||
        (!p && length)) return -1;
    while (length) {
        if (r->header_used < sizeof(r->header)) {
            amount = sizeof(r->header) - r->header_used;
            if (amount > length) amount = length;
            memcpy(r->header + r->header_used, p, amount);
            r->header_used += amount; p += amount; length -= amount;
            if (r->header_used < sizeof(r->header)) continue;
            r->body_size = (size_t)r->header[0] | (size_t)r->header[1]<<8 |
                           (size_t)r->header[2]<<16 | (size_t)r->header[3]<<24;
            if (r->header[4] > 5 || r->body_size > 512000 || r->body_size > r->body_capacity)
                return fail(r, now);
            if (!r->body_size) { if (message(r, now)) return -1; continue; }
        }
        amount = r->body_size - r->body_used;
        if (amount > length) amount = length;
        memcpy(r->body + r->body_used, p, amount);
        r->body_used += amount; p += amount; length -= amount;
        if (r->body_used == r->body_size && message(r, now)) return -1;
    }
    return 0;
}
int pcm4_screen_receiver_end(struct pcm4_screen_receiver *r, uint64_t generation, uint64_t now)
{
    int partial, rc;
    if (!r || !r->projection || r->generation != generation ||
        r->projection->stream.generation != generation) return -1;
    partial = r->header_used != 0 || r->body_used != 0 || r->failed;
    r->failed = 1;
    rc = pcm4_projection_end(r->projection, generation, now);
    return partial || rc ? -1 : 0;
}

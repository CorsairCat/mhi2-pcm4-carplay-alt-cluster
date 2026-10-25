#ifndef PCM4_SCREEN_RECEIVER_H
#define PCM4_SCREEN_RECEIVER_H
#include "pcm4_projection.h"

/* Incremental stream-111 Screen message framing. Original implementation.
 * Wire layout follows the public Audi receiver: 128-byte header, LE body size,
 * opcode byte 4. Only VIDEO_FRAME bodies are decrypted, even before config.
 * One serialized owner; buffers must be disjoint and caller-owned. No sockets,
 * session key interception, negotiation, inline hooks or guessed AES ABI here.
 */
struct pcm4_screen_receiver {
    struct pcm4_projection *projection;
    uint64_t generation;
    unsigned char header[128];
    size_t header_used, body_used, body_size;
    unsigned char *body, *scratch;
    size_t body_capacity, scratch_capacity;
    int (*decrypt_frame)(void *, void *, size_t);
    void *crypto;
    unsigned failed;
    uint64_t messages, accepted_frames;
};
int pcm4_screen_receiver_init(struct pcm4_screen_receiver *, struct pcm4_projection *,
                              uint64_t generation, void *body, size_t,
                              void *scratch, size_t,
                              int (*decrypt_frame)(void *, void *, size_t), void *crypto);
int pcm4_screen_receiver_feed(struct pcm4_screen_receiver *, uint64_t generation,
                              const void *, size_t, uint64_t now_ms);
/* End session. Returns -1 for a truncated final message or cleanup failure. */
int pcm4_screen_receiver_end(struct pcm4_screen_receiver *, uint64_t generation, uint64_t now_ms);
#endif

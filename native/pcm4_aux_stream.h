#ifndef PCM4_AUX_STREAM_H
#define PCM4_AUX_STREAM_H
#include <stddef.h>
#include <stdint.h>

/* Original post-decryption adapter, NOT an AirPlay receiver or network parser.
 * One owner, serialized calls, no reentrant callbacks. Session generations are
 * strictly increasing; old socket/session callbacks cannot feed a new decoder.
 * Scratch belongs to caller and must be >=512000 bytes, disjoint from input.
 */
struct pcm4_aux_stream {
    uint64_t generation;
    unsigned nal_length_size, configured, primed;
    unsigned char parameters[8192];
    size_t parameters_size;
    int (*emit)(void *, const void *, size_t, uint64_t);
    void *opaque;
};
int pcm4_aux_begin(struct pcm4_aux_stream *, uint64_t,
                   int (*)(void *, const void *, size_t, uint64_t), void *);
int pcm4_aux_configure(struct pcm4_aux_stream *, uint64_t, const void *, size_t);
/* 0 emitted; 1 waiting for config/IDR; -1 malformed/stale or emit failure.
 * Input is ONE complete, decrypted AVCC access unit, not arbitrary TCP data.
 * Timestamp units must be adapted by the eventual receiver/renderer bridge.
 */
int pcm4_aux_access_unit(struct pcm4_aux_stream *, uint64_t, const void *, size_t,
                         uint64_t, void *scratch, size_t);
void pcm4_aux_end(struct pcm4_aux_stream *, uint64_t);
#endif

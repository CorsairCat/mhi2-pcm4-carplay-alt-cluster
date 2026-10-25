#ifndef PCM4_READY_H
#define PCM4_READY_H
#include <stdint.h>
/* Atomic, frame-driven readiness. Serialized owner; path in writable RAM. */
struct pcm4_ready { char path[512]; unsigned sequence; };
int pcm4_ready_init(struct pcm4_ready *, const char *absolute_path);
/* frames==0 withdraws; positive values must fit Java signed long. */
int pcm4_ready_publish(void *, uint64_t generation, uint64_t frames);
#endif

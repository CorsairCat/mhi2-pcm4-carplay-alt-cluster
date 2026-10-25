#ifndef PCM4_CRYPTO_H
#define PCM4_CRYPTO_H
#include <stddef.h>
#include <stdint.h>
struct pcm4_crypto_api {
    void (*derive)(const void *, size_t, uint64_t, unsigned char *, unsigned char *);
    int (*init)(void *, const unsigned char *, const unsigned char *);
    int (*update)(void *, const void *, size_t, void *);
    void (*final)(void *);
};
struct pcm4_crypto {
    union { uint64_t alignment; unsigned char bytes[512]; } state;
    struct pcm4_crypto_api api;
    unsigned initialized;
};
int pcm4_crypto_begin(struct pcm4_crypto *, const struct pcm4_crypto_api *,
                       const unsigned char master[16], uint64_t connection_id);
int pcm4_crypto_decrypt(void *, void *, size_t);
void pcm4_crypto_end(struct pcm4_crypto *);
#endif

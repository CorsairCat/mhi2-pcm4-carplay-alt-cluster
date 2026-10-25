#ifndef PCM4_IDENTITY_H
#define PCM4_IDENTITY_H
/* Streaming SHA-256 file identity; reads bytes only, never dlopens. */
int pcm4_identity_file(const char *path,const char *expected_hex);
#endif

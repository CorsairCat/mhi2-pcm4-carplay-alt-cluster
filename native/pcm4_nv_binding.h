#ifndef PCM4_NV_BINDING_H
#define PCM4_NV_BINDING_H
#include "pcm4_mu1476_display.h"

/* Owns one dlopen reference. Zero initialize before first use. Caller must
 * close every renderer using api BEFORE unloading. No concurrent operations.
 * Loading a library can execute its constructors; this is not an offline
 * inspection interface. The future launcher must verify target file identity.
 */
struct pcm4_nv_binding {
    void *module;
    struct pcm4_nv_api api;
    char error[192];
};
int pcm4_nv_bind(struct pcm4_nv_binding *, const char *absolute_library_path);
int pcm4_nv_unbind(struct pcm4_nv_binding *);
#endif

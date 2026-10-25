#include "pcm4_nv_binding.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

static void error(struct pcm4_nv_binding *b, const char *symbol)
{
    const char *detail = dlerror();
    snprintf(b->error, sizeof(b->error), "%s: %s", symbol, detail ? detail : "null address");
}

int pcm4_nv_bind(struct pcm4_nv_binding *b, const char *path)
{
    void *address;
    if (!b || b->module || !path || path[0] != '/') return -1;
    memset(&b->api, 0, sizeof(b->api));
    b->error[0] = '\0';
    b->module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!b->module) { error(b, path); return -1; }
    /* memcpy avoids type-punning writes through a void **. The QNX ARM ABI
     * uses equally sized object/function pointers, asserted below per field. */
#define LOAD(field, symbol) do { \
    typedef char same_size_##field[sizeof(b->api.field) == sizeof(address) ? 1 : -1]; \
    (void)sizeof(same_size_##field); \
    dlerror(); address = dlsym(b->module, symbol); \
    if (!address) { error(b, symbol); goto fail; } \
    memcpy(&b->api.field, &address, sizeof(address)); \
} while (0)
    LOAD(open, "NvSSVideoOpen");
    LOAD(configure, "NvSSVideoStreamConfigure");
    LOAD(get_attributes, "NvSSVideoGetAttribs");
    LOAD(set_attributes, "NvSSVideoSetAttribs");
    LOAD(decode, "NvSSVideoDecode");
    LOAD(close, "NvSSVideoClose");
#undef LOAD
    return 0;
fail:
    dlclose(b->module);
    b->module = NULL;
    memset(&b->api, 0, sizeof(b->api));
    return -1;
}

int pcm4_nv_unbind(struct pcm4_nv_binding *b)
{
    if (!b) return -1;
    if (b->module && dlclose(b->module) != 0) { error(b, "dlclose"); return -1; }
    b->module = NULL;
    memset(&b->api, 0, sizeof(b->api));
    return 0;
}

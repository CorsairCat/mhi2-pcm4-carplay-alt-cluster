/* Synthetic host library only. No vendor code or hardware calls. */
#include "pcm4_mu1476_display.h"
#include <string.h>
static int object;
int NvSSVideoOpen(void **out, const struct pcm4_nv_open *params)
{
    if (!out || params->stream_type != 0) return -9;
    *out = &object;
    return 0;
}
int NvSSVideoStreamConfigure(void *handle, const struct pcm4_nv_config *config)
{ return handle == &object && config->width == 448 ? 0 : -9; }
int NvSSVideoGetAttribs(void *handle, void *attributes)
{ (void)handle; memset(attributes, 0, 64); return 0; }
int NvSSVideoSetAttribs(void *handle, const void *attributes)
{
    const unsigned char *p = attributes;
    if (handle != &object) return -9;
    return (p[8] == 1 || (p[60] == 1 && p[61] <= 1)) ? 0 : -9;
}
int NvSSVideoDecode(void *handle, const struct pcm4_nv_input *input, int flags)
{ (void)handle; (void)flags; return input->length ? 0 : -9; }
#ifndef PCM4_MOCK_MISSING_CLOSE
int NvSSVideoClose(void *handle) { return handle == &object ? 0 : -9; }
#endif

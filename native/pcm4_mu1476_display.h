/* Original MU1476 renderer core. See PCM4_MU1476_DISPLAY_PORT.md.
 * No installer, HMI ownership or CarPlay transport is included.
 */
#ifndef PCM4_MU1476_DISPLAY_H
#define PCM4_MU1476_DISPLAY_H
#include <stddef.h>
#include <stdint.h>

#if !defined(PCM4_HOST_TEST)
#if !defined(__QNXNTO__) || !defined(__arm__)
#error Build the target core with a QNX ARM toolchain, or use PCM4_HOST_TEST for mocks.
#endif
#ifdef __ARMEB__
#error The reviewed library uses little-endian ARM.
#endif
typedef char pcm4_pointer_is_32_bits[sizeof(void *) == 4 ? 1 : -1];
#endif

/* Field names are provisional where vendor declarations have not been found. */
struct pcm4_nv_open {
    uint32_t output_device, reserved_at_4;
    uint8_t priority, use_layer, layer, reserved_at_11;
    uint32_t reserved_at_12, stream_type, word_at_20;
};
struct pcm4_nv_config { uint32_t width, height; float fps; };
struct pcm4_nv_input {
    const void *data;
    uint32_t length;
    uint64_t timestamp;
    uint8_t flag, reserved[7];
};
typedef char pcm4_open_is_24[sizeof(struct pcm4_nv_open) == 24 ? 1 : -1];
typedef char pcm4_stream_type_at_16[offsetof(struct pcm4_nv_open, stream_type) == 16 ? 1 : -1];
typedef char pcm4_config_is_12[sizeof(struct pcm4_nv_config) == 12 ? 1 : -1];
#ifndef PCM4_HOST_TEST
typedef char pcm4_timestamp_at_8[offsetof(struct pcm4_nv_input, timestamp) == 8 ? 1 : -1];
typedef char pcm4_flag_at_16[offsetof(struct pcm4_nv_input, flag) == 16 ? 1 : -1];
typedef char pcm4_input_is_24[sizeof(struct pcm4_nv_input) == 24 ? 1 : -1];
#endif

struct pcm4_nv_api {
    int (*open)(void **, const struct pcm4_nv_open *);
    int (*configure)(void *, const struct pcm4_nv_config *);
    int (*get_attributes)(void *, void *);
    int (*set_attributes)(void *, const void *);
    int (*decode)(void *, const struct pcm4_nv_input *, int);
    int (*close)(void *);
};
struct pcm4_rect { uint32_t x, y, width, height; };
struct pcm4_display_config {
    uint32_t output_device, output_width, output_height;
    uint8_t priority, layer;
    struct pcm4_nv_config video;
    struct pcm4_rect destination;
};
/* Must be zero-initialized by the caller; one owner, no concurrent calls. */
struct pcm4_display {
    const struct pcm4_nv_api *api;
    void *handle;
};
/* 0 success; -1 invalid input/state; other failures preserve vendor status. */
int pcm4_display_open(struct pcm4_display *, const struct pcm4_nv_api *,
                      const struct pcm4_display_config *);
int pcm4_display_decode(struct pcm4_display *, const void *, size_t, uint64_t);
/* Explicit output binding control; does not select/own an HMI context. Static
 * MU1476 trace only, not validated on the vehicle. visible must be 0 or 1.
 * Do not copy the queried byte 61 back as a setter: it has inverted polarity. */
int pcm4_display_set_visible(struct pcm4_display *, int visible);
int pcm4_display_close(struct pcm4_display *);
#endif

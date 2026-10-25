/* Host mocks exercise lifecycle and bounds, never load vendor libraries. */
#include "pcm4_mu1476_display.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int fail_stage, stage, closed, decoded;
static unsigned char attributes[64];
static struct pcm4_nv_open opening;
static int token;
static int next(void) { return ++stage == fail_stage ? -77 : 0; }
static int open_video(void **handle, const struct pcm4_nv_open *p)
{
    int rc = next();
    opening = *p;
    if (!rc) *handle = &token;
    return rc;
}
static int configure(void *handle, const struct pcm4_nv_config *c)
{
    assert(handle == &token && c->width == 448 && c->height == 448 && c->fps == 30);
    return next();
}
static int get_attributes(void *handle, void *p)
{
    assert(handle == &token);
    /* Simulate query fields while preserving zeroed command flags. */
    ((unsigned char *)p)[61] = 1;
    return next();
}
static int set_attributes(void *handle, const void *p)
{
    assert(handle == &token);
    memcpy(attributes, p, sizeof(attributes));
    return next();
}
static int decode(void *handle, const struct pcm4_nv_input *in, int flags)
{
    assert(handle == &token && flags == 0 && in->length == 4);
    assert(in->timestamp == 123 && in->flag == 0);
    assert(memcmp(in->data, "test", 4) == 0);
    decoded++;
    return 0;
}
static int close_video(void *handle) { assert(handle == &token); closed++; return 0; }
static const struct pcm4_nv_api api = {
    open_video, configure, get_attributes,
    set_attributes, decode, close_video
};
static uint32_t word(unsigned at)
{
    return (uint32_t)attributes[at] | ((uint32_t)attributes[at+1] << 8) |
           ((uint32_t)attributes[at+2] << 16) | ((uint32_t)attributes[at+3] << 24);
}
static void reset(void) { stage = closed = decoded = fail_stage = 0; }

int main(void)
{
    /* Synthetic config exercises nonzero viewport offsets; these IDs and this
     * rectangle are test inputs, NOT selected vehicle parameters. */
    struct pcm4_display_config c = {1, 448, 448, 100, 60,
                                   {448, 448, 30}, {20, 30, 290, 330}};
    struct pcm4_display d = {0};
    int i;
    assert(pcm4_display_open(&d, &api, &c) == 0);
    assert(stage == 4 && opening.stream_type == 0 && opening.word_at_20 == 0);
    assert(opening.output_device == 1 && opening.layer == 60 && opening.use_layer == 1);
    assert(attributes[8] == 1 && attributes[44] == 0 && attributes[60] == 0 && attributes[61] == 1);
    assert(word(12) == 0 && word(16) == 0 && word(20) == 448 && word(24) == 448);
    assert(word(28) == 20 && word(32) == 30 && word(36) == 290 && word(40) == 330);
    assert(pcm4_display_open(&d, &api, &c) == -1); /* no double initialization */
    assert(pcm4_display_decode(&d, "test", 4, 123) == 0 && decoded == 1);
    assert(pcm4_display_decode(&d, "test", 512001, 0) == -1 && decoded == 1);
    assert(pcm4_display_decode(&d, NULL, 4, 0) == -1);
    assert(pcm4_display_decode(&d, "test", 0, 0) == -1);
    assert(pcm4_display_set_visible(&d, 1) == 0);
    for (i = 0; i < 64; i++) assert(attributes[i] == (i == 60));
    assert(pcm4_display_set_visible(&d, 0) == 0);
    for (i = 0; i < 64; i++) assert(attributes[i] == (i == 60 || i == 61));
    i = stage;
    assert(pcm4_display_set_visible(&d, 2) == -1 && stage == i);
    assert(pcm4_display_set_visible(&d, -1) == -1 && stage == i);
    fail_stage = stage + 1;
    assert(pcm4_display_set_visible(&d, 1) == -77);
    assert(d.handle == &token && closed == 0); /* failure does not discard owner */
    assert(pcm4_display_close(&d) == 0 && closed == 1);
    assert(pcm4_display_close(&d) == 0 && closed == 1);
    assert(pcm4_display_decode(&d, "test", 4, 123) == -1);
    assert(pcm4_display_set_visible(&d, 1) == -1);
    assert(pcm4_display_set_visible(NULL, 1) == -1);
    for (i = 1; i <= 4; i++) {
        reset(); fail_stage = i;
        assert(pcm4_display_open(&d, &api, &c) == -77);
        assert(stage == i && closed == (i > 1));
        assert(d.api == NULL && d.handle == NULL);
    }
    reset(); c.destination.x = UINT32_MAX;
    assert(pcm4_display_open(&d, &api, &c) == -1 && stage == 0);
    c.destination.x = 20; c.destination.width = 429;
    assert(pcm4_display_open(&d, &api, &c) == -1 && stage == 0);
    c.destination.width = 290; c.output_device = 4;
    assert(pcm4_display_open(&d, &api, &c) == -1 && stage == 0);
    c.output_device = 1; c.video.fps = 0;
    assert(pcm4_display_open(&d, &api, &c) == -1 && stage == 0);
    puts("PASS: lifecycle, four failure stages, geometry, frame bounds and visibility polarity/failure");
    return 0;
}

/* Original bounded sample driver. Never selects a stock HMI context.
 * Timestamps passed to NvSS are zero until target timestamp units are verified.
 */
#define _POSIX_C_SOURCE 200112L
#include "pcm4_nv_binding.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile sig_atomic_t stop;
static void interrupted(int sig) { (void)sig; stop = 1; }
static uint32_t le32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static int number(const char *s, uint32_t *n)
{
    char *end;
    unsigned long value;
    if (!s[0] || s[0] < '0' || s[0] > '9') return -1;
    errno = 0; value = strtoul(s, &end, 10);
    if (errno || *end || value > 0xffffffffUL) return -1;
    *n = (uint32_t)value; return 0;
}
static int record(FILE *file, unsigned char *data, uint32_t *size)
{
    unsigned char bytes[4];
    if (fread(bytes, 1, 4, file) != 4) return -1;
    *size = le32(bytes);
    if (*size < 5 || *size > 512000 || fread(data, 1, *size, file) != *size) return -1;
    if (memcmp(data, "\0\0\0\1", 4) && memcmp(data, "\0\0\1", 3)) return -1;
    return 0;
}
int main(int argc, char **argv)
{
    struct pcm4_display_config config;
    struct pcm4_nv_binding binding = {0};
    struct pcm4_display display = {0};
    unsigned char header[24], *data = NULL;
    uint32_t values[9], frames, fps, size, i;
    FILE *file = NULL;
    int rc = 1, cleanup;
    if (argc != 13 || strcmp(argv[1], "--mapping-and-identity-reviewed")) {
        fprintf(stderr, "usage: %s --mapping-and-identity-reviewed /absolute/libnvss_video.so sample.pc4t output layer priority outputW outputH destX destY destW destH\n", argv[0]);
        return 2;
    }
    for (i = 0; i < 9; ++i) if (number(argv[i+4], &values[i])) return 2;
    if (values[0] > 1 || values[1] > 255 || values[2] > 255) return 2;
    memset(&config, 0, sizeof(config));
    config.output_device = values[0]; config.layer = (uint8_t)values[1];
    config.priority = (uint8_t)values[2];
    config.output_width = values[3]; config.output_height = values[4];
    config.destination.x = values[5]; config.destination.y = values[6];
    config.destination.width = values[7]; config.destination.height = values[8];
    file = fopen(argv[3], "rb");
    if (!file || fread(header, 1, 24, file) != 24 || memcmp(header, "PC4T", 4) || le32(header+4) != 1) goto done;
    config.video.width = le32(header+8); config.video.height = le32(header+12);
    fps = le32(header+16); frames = le32(header+20);
    if (!config.video.width || config.video.width > 1024 || !config.video.height ||
        config.video.height > 1024 || !fps || fps > 30 || !frames || frames > fps*5) goto done;
    config.video.fps = (float)fps;
    data = malloc(512000);
    if (!data) goto done;
    /* Validate all lengths and EOF before any vendor constructors execute. */
    for (i = 0; i < frames; ++i) if (record(file, data, &size)) goto done;
    if (fgetc(file) != EOF || ferror(file) || fseek(file, 24, SEEK_SET)) goto done;
    signal(SIGINT, interrupted); signal(SIGTERM, interrupted);
    if (pcm4_nv_bind(&binding, argv[2])) { fprintf(stderr, "%s\n", binding.error); goto done; }
    if (stop || pcm4_display_open(&display, &binding.api, &config)) goto done;
    for (i = 0; i < frames && !stop; ++i) {
        struct timespec delay;
        if (record(file, data, &size) || pcm4_display_decode(&display, data, size, 0)) goto done;
        if (i == 0 && pcm4_display_set_visible(&display, 1)) goto done;
        delay.tv_sec = 0; delay.tv_nsec = 1000000000L / fps;
        while (!stop && nanosleep(&delay, &delay) && errno == EINTR) { }
    }
    rc = stop ? 130 : 0;
done:
    if (display.handle) {
        cleanup = pcm4_display_set_visible(&display, 0);
        if (cleanup) { fprintf(stderr, "hide failed: %d\n", cleanup); rc = 1; }
        cleanup = pcm4_display_close(&display);
        if (cleanup) { fprintf(stderr, "close failed: %d\n", cleanup); rc = 1; }
    }
    cleanup = pcm4_nv_unbind(&binding);
    if (cleanup) { fprintf(stderr, "unload failed: %d\n", cleanup); rc = 1; }
    if (file) fclose(file);
    free(data);
    fprintf(stderr, "Controlled test ended: %d (0 reports API success, not visible pixels).\n", rc);
    return rc;
}

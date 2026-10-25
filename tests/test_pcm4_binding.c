#include "pcm4_nv_binding.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    struct pcm4_nv_binding b = {0};
    struct pcm4_display d = {0};
    struct pcm4_display_config c = {1, 448, 448, 100, 60, {448, 448, 30}, {0, 0, 448, 448}};
    assert(argc == 3);
    assert(pcm4_nv_bind(&b, "relative.so") == -1 && !b.module);
    assert(pcm4_nv_bind(&b, "/nonexistent/pcm4-mock.so") == -1 && !b.module && b.error[0]);
    assert(pcm4_nv_bind(&b, argv[2]) == -1 && !b.module);
    assert(strstr(b.error, "NvSSVideoClose") && !b.api.open && !b.api.decode);
    assert(pcm4_nv_bind(&b, argv[1]) == 0 && b.module && b.api.decode);
    assert(pcm4_nv_bind(&b, argv[1]) == -1); /* retain existing binding */
    assert(pcm4_display_open(&d, &b.api, &c) == 0);
    assert(pcm4_display_decode(&d, "test", 4, 0) == 0);
    assert(pcm4_display_close(&d) == 0);
    assert(pcm4_nv_unbind(&b) == 0 && !b.module && !b.api.open);
    assert(pcm4_nv_unbind(&b) == 0);
    puts("PASS: synthetic dynamic binding, missing symbols, core integration and unload");
    return 0;
}

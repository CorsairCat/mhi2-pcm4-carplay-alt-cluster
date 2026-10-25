#!/bin/sh
# Run on a licensed, compatible QNX SDK development host, never on the car.
set -eu
cd "$(dirname "$0")"
compiler=${PCM4_QCC:-qcc}
variant=${PCM4_QCC_VARIANT:-gcc_ntoarmv7le}
command -v "$compiler" >/dev/null 2>&1 || {
    echo 'No QNX compiler. Set PCM4_QCC and source your SDK environment.' >&2
    exit 2
}
mkdir -p bin
"$compiler" -V"$variant" -std=c99 -Wall -Wextra -Werror -Isrc \
    src/pcm4_mu1476_display.c src/pcm4_nv_binding.c src/render_test.c \
    -o bin/pcm4-render-test
echo 'Built bin/pcm4-render-test; verify its ARM32 ABI/runtime before deployment.'

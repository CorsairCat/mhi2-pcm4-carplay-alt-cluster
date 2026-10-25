#!/bin/sh
# Original code, synthetic access units and mock NvSS only.
set -eu
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/pcm4-projection.XXXXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${PCM4_CC:-clang}" -std=c99 -Wall -Wextra -Werror -DPCM4_HOST_TEST -Inative \
    -fsanitize=address,undefined native/pcm4_mu1476_display.c \
    native/pcm4_aux_stream.c native/pcm4_handover.c native/pcm4_projection.c native/pcm4_screen_receiver.c \
    tests/test_pcm4_projection.c -o "$test_dir/test"
"$test_dir/test"

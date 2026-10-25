#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/pcm4-runtime.XXXXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
cc=${PCM4_CC:-clang}
common='-std=c99 -Wall -Wextra -Werror -DPCM4_HOST_TEST -Inative -fsanitize=address,undefined'
$cc $common -Wno-unused-function native/pcm4_crypto.c native/pcm4_identity.c \
    native/pcm4_arm_hook.c tests/test_pcm4_hook.c -o "$run_dir/hook-test"
"$run_dir/hook-test"
$cc $common native/pcm4_mu1476_display.c native/pcm4_nv_binding.c native/pcm4_aux_stream.c \
    native/pcm4_handover.c native/pcm4_projection.c native/pcm4_screen_receiver.c \
    native/pcm4_ready.c native/pcm4_identity.c native/pcm4_projection_runtime.c -o "$run_dir/runtime"
$cc -std=c99 -Wall -Wextra -Werror -DPCM4_HOST_TEST -Inative -shared -fPIC \
    tests/mock_nvss_library.c -o "$run_dir/mock.so"
python3 tests/test_pcm4_runtime.py "$run_dir/runtime" "$run_dir/mock.so"

$cc -std=c99 -Wall -Wextra -Werror -fPIC -shared native/pcm4_identity.c -o "$run_dir/identity.so"
python3 tests/test_pcm4_identity.py "$run_dir/identity.so"

#!/bin/sh
# Run INSIDE the QNX 6.5 toolchain container from the original-source checkout.
# Does not execute any target binary or load vendor libraries.
set -eu
cd "$(dirname "$0")/.."
compiler=${PCM4_QNX_CC:-arm-unknown-nto-qnx6.5.0eabi-gcc}
reader=${PCM4_QNX_READELF:-arm-unknown-nto-qnx6.5.0eabi-readelf}
mkdir -p controlled-test/bin qnx-out
"$compiler" --version >qnx-out/compiler.txt
"$compiler" -O0 -std=c99 -Wall -Wextra -Werror -marm -mfloat-abi=softfp \
    -static-libgcc -Icontrolled-test/src controlled-test/src/pcm4_mu1476_display.c \
    controlled-test/src/pcm4_nv_binding.c controlled-test/src/render_test.c \
    -o controlled-test/bin/pcm4-render-test
"$compiler" -O0 -std=c99 -Wall -Wextra -Werror -marm -mfloat-abi=softfp \
    -static-libgcc -Inative native/pcm4_mu1476_display.c tests/test_pcm4_display.c \
    -o qnx-out/display-mock-test
"$compiler" -O0 -std=c99 -Wall -Wextra -Werror -marm -mfloat-abi=softfp \
    -static-libgcc -Inative native/pcm4_mu1476_display.c native/pcm4_aux_stream.c \
    native/pcm4_handover.c native/pcm4_projection.c native/pcm4_screen_receiver.c tests/test_pcm4_projection.c \
    -o qnx-out/projection-mock-test
"$compiler" -O0 -std=c99 -Wall -Wextra -Werror -marm -mfloat-abi=softfp \
    -static-libgcc -fPIC -shared -Inative tests/mock_nvss_library.c -o qnx-out/mock.so
"$compiler" -O0 -std=c99 -Wall -Wextra -Werror -marm -mfloat-abi=softfp \
    -static-libgcc -DPCM4_MOCK_MISSING_CLOSE -fPIC -shared -Inative \
    tests/mock_nvss_library.c -o qnx-out/missing.so
"$compiler" -O0 -std=c99 -Wall -Wextra -Werror -marm -mfloat-abi=softfp \
    -static-libgcc -Inative native/pcm4_mu1476_display.c native/pcm4_nv_binding.c \
    tests/test_pcm4_binding.c -o qnx-out/binding-mock-test
for binary in controlled-test/bin/pcm4-render-test qnx-out/display-mock-test \
    qnx-out/projection-mock-test qnx-out/binding-mock-test qnx-out/mock.so qnx-out/missing.so; do
    "$reader" -h -l -d -A "$binary" >"qnx-out/$(basename "$binary").elf.txt"
done
sha256sum controlled-test/bin/pcm4-render-test qnx-out/*-test qnx-out/*.so >qnx-out/SHA256SUMS
echo 'Cross-compilation complete. Target execution and display mapping remain unverified.'

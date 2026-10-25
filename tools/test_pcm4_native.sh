#!/bin/sh
# Build and run ORIGINAL host mock code. Never loads the vendor ELF libraries.
set -eu
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/pcm4-native.XXXXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
compiler=${PCM4_CC:-clang}
case "$(uname -s)" in
    Darwin) shared_flag=-dynamiclib; loader_flag= ;;
    *) shared_flag=-shared; loader_flag=-ldl ;;
esac
"$compiler" -std=c99 -Wall -Wextra -Werror -DPCM4_HOST_TEST -Inative \
    -fsanitize=address,undefined native/pcm4_mu1476_display.c tests/test_pcm4_display.c \
    -o "$test_dir/core-test"
"$test_dir/core-test"
"$compiler" -std=c99 -Wall -Wextra -Werror -DPCM4_HOST_TEST -Inative -fPIC \
    "$shared_flag" tests/mock_nvss_library.c -o "$test_dir/mock.so"
"$compiler" -std=c99 -Wall -Wextra -Werror -DPCM4_HOST_TEST -Inative -fPIC \
    -DPCM4_MOCK_MISSING_CLOSE "$shared_flag" tests/mock_nvss_library.c -o "$test_dir/missing.so"
"$compiler" -std=c99 -Wall -Wextra -Werror -DPCM4_HOST_TEST -Inative \
    -fsanitize=address,undefined native/pcm4_mu1476_display.c native/pcm4_nv_binding.c \
    tests/test_pcm4_binding.c ${loader_flag:+"$loader_flag"} -o "$test_dir/binding-test"
"$test_dir/binding-test" "$test_dir/mock.so" "$test_dir/missing.so"

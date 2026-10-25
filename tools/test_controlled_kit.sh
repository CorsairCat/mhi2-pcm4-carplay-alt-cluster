#!/bin/sh
# Uses generated original pattern + synthetic library; no vendor ELF loaded.
set -eu
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/pcm4-kit.XXXXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
compiler=${PCM4_CC:-clang}
case "$(uname -s)" in
    Darwin) shared_flag=-dynamiclib; loader_flag= ;;
    *) shared_flag=-shared; loader_flag=-ldl ;;
esac
for name in pcm4_mu1476_display.c pcm4_mu1476_display.h pcm4_nv_binding.c pcm4_nv_binding.h; do
    cmp "native/$name" "controlled-test/src/$name"
done
cmp profiles/pcm4-mu1476-display.json controlled-test/reference-profile.json
python3 controlled-test/make_sample.py
"$compiler" -std=c99 -Wall -Wextra -Werror -DPCM4_HOST_TEST -Inative -fPIC \
    "$shared_flag" tests/mock_nvss_library.c -o "$test_dir/mock.so"
"$compiler" -std=c99 -Wall -Wextra -Werror -DPCM4_HOST_TEST -Icontrolled-test/src \
    -fsanitize=address,undefined controlled-test/src/pcm4_mu1476_display.c \
    controlled-test/src/pcm4_nv_binding.c controlled-test/src/render_test.c \
    ${loader_flag:+"$loader_flag"} -o "$test_dir/driver"
"$test_dir/driver" --mapping-and-identity-reviewed "$test_dir/mock.so" \
    controlled-test/media/pattern.pc4t 1 60 100 448 448 0 0 448 448
# Extra bytes must be rejected before library loading, even with a valid header.
cp controlled-test/media/pattern.pc4t "$test_dir/trailing.pc4t"
printf x >>"$test_dir/trailing.pc4t"
if "$test_dir/driver" --mapping-and-identity-reviewed "$test_dir/mock.so" \
    "$test_dir/trailing.pc4t" 1 60 100 448 448 0 0 448 448; then
    echo 'FAIL: accepted trailing bytes' >&2; exit 1
fi
if "$test_dir/driver" --mapping-and-identity-reviewed "$test_dir/mock.so" \
    controlled-test/media/pattern.pc4t 2 60 100 448 448 0 0 448 448; then
    echo 'FAIL: accepted output device 2' >&2; exit 1
fi
echo 'PASS: kit snapshot, full mock playback/cleanup, malformed file and invalid device'

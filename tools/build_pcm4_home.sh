#!/bin/bash
# Send only original sources to the owner-authorized host alias "home".
set -euo pipefail
cd "$(dirname "$0")/.."
tar -cf - controlled-test/src native tests/test_pcm4_projection.c \
    tests/test_pcm4_display.c tests/test_pcm4_binding.c tests/mock_nvss_library.c \
    tools/build_pcm4_qnx.sh | ssh -o BatchMode=yes home \
    'mkdir -p ~/dev/pcm4-qnx-toolchain/pcm4-project; tar -xf - -C ~/dev/pcm4-qnx-toolchain/pcm4-project'
ssh -o BatchMode=yes home 'bash -s' <<'REMOTE'
set -euo pipefail
cd "$HOME/dev/pcm4-qnx-toolchain"
[ "$(cat setup.status)" = complete ] || { echo 'Toolchain setup is not complete.' >&2; exit 2; }
image=$(cat image-tag.txt)
sudo -n docker run --rm --network none --read-only \
    --tmpfs /tmp:rw,nosuid,nodev,size=256m --cap-drop ALL \
    --security-opt no-new-privileges --user "$(id -u):$(id -g)" \
    --mount "type=bind,src=$PWD/pcm4-project,dst=/src" \
    "$image" sh tools/build_pcm4_qnx.sh
cp image-tag.txt image-id.txt source-revision.txt pcm4-project/qnx-out/
REMOTE
mkdir -p controlled-test/bin private/remote-qnx
scp home:dev/pcm4-qnx-toolchain/pcm4-project/controlled-test/bin/pcm4-render-test \
    controlled-test/bin/pcm4-render-test
scp -r home:dev/pcm4-qnx-toolchain/pcm4-project/qnx-out/. private/remote-qnx/
mkdir -p controlled-test/bin/qnx-mocks
for name in display-mock-test projection-mock-test binding-mock-test mock.so missing.so; do
    cp "private/remote-qnx/$name" "controlled-test/bin/qnx-mocks/$name"
done
vehicle_libc=${PCM4_VEHICLE_LIBC:-private/analysis/mu1476-libc/files/proc/boot/libc.so.3}
if [ -f "$vehicle_libc" ]; then
    python3 tools/check_qnx_target.py controlled-test/bin/pcm4-render-test \
        --libc "$vehicle_libc" --report private/remote-qnx/vehicle-libc-check.json
else
    echo 'Vehicle libc unavailable: static import gate has NOT run.' >&2
fi
echo 'Target binary: controlled-test/bin/pcm4-render-test; reports: private/remote-qnx/'

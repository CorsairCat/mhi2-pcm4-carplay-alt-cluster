#!/bin/bash
# Run on the authorized Debian Docker host as the normal login user.
# All project files live in ~/dev/pcm4-qnx-toolchain; no host packages installed.
set -euo pipefail
workspace="$HOME/dev/pcm4-qnx-toolchain"
revision=baa45224f18ae6b13c2e9c77c6663bb8181eb6a1
image="pcm4-qnx65-armv7:8.5-${revision:0:12}"
mkdir -p "$workspace"
cd "$workspace"
exec 9>setup.lock
flock -n 9 || { echo 'Another setup is already running.' >&2; exit 2; }
printf 'running\n' >setup.status
trap 'result=$?; if [ "$result" -eq 0 ]; then echo complete >setup.status; else echo "failed:$result" >setup.status; fi' EXIT
for tool in git docker python3; do command -v "$tool" >/dev/null; done
docker_cmd=(docker)
if ! docker info >/dev/null 2>&1; then docker_cmd=(sudo -n docker); fi
"${docker_cmd[@]}" info >/dev/null
if [ ! -e source ]; then
    git clone https://github.com/luka-dev/qnx65-armv7-toolchain.git source
fi
[ "$(git -C source remote get-url origin)" = https://github.com/luka-dev/qnx65-armv7-toolchain.git ]
git -C source cat-file -e "$revision^{commit}"
context="build-context-${revision:0:12}-jobs4"
if [ ! -e "$context" ]; then
    mkdir "$context"
    # git archive preserves the real symlinks needed by QNX binutils.
    git -C source archive "$revision" | tar -x -C "$context"
    python3 - "$context" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1])
changes = {'gcc/build.sh': ('J=$(nproc)', 'J=4'),
           'binutils/build.sh': ('make -j"$(nproc)"', 'make -j2')}
for name, (before, after) in changes.items():
    path = root / name
    text = path.read_text()
    if text.count(before) != 1:
        raise SystemExit('Unexpected upstream job setting: ' + name)
    path.write_text(text.replace(before, after))
PY
    git -C source diff --exit-code
    printf '%s\n' "$revision" >"$context/.pcm4-revision"
fi
[ "$(cat "$context/.pcm4-revision")" = "$revision" ]
# Debian snapshot mirrors occasionally reset package downloads. Retry within
# this image only; retain upstream snapshot, signatures and archive hashes.
python3 - "$context/Dockerfile" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text()
marker = '# PCM4 container download retries'
if marker not in text:
    before = 'RUN dpkg --add-architecture i386'
    if text.count(before) != 1:
        raise SystemExit('Unexpected Dockerfile dependency stage')
    text = text.replace(before, marker + '\nRUN echo \'Acquire::Retries "5";\' > /etc/apt/apt.conf.d/80pcm4-retries\n' + before)
    text = text.replace('curl -fsSL', 'curl --retry 5 --retry-delay 2 --retry-all-errors -fsSL')
    path.write_text(text)
PY
printf '%s\n' "$image" >image-tag.txt
printf '%s\n' "$revision" >source-revision.txt
"${docker_cmd[@]}" build --progress=plain --platform=linux/amd64 \
    --target base-env --build-arg BASE=base-8.5 \
    --label "org.opencontainers.image.revision=$revision" \
    --label 'pcm4.local-build-changes=gcc-jobs4,gas-jobs2,download-retries5' \
    -t "$image" "$context"
"${docker_cmd[@]}" run --rm --network none "$image" \
    arm-unknown-nto-qnx6.5.0eabi-gcc --version
"${docker_cmd[@]}" run --rm --network none "$image" \
    arm-unknown-nto-qnx6.5.0eabi-as --version
"${docker_cmd[@]}" image inspect --format '{{.Id}}' "$image" >image-id.txt
echo 'Toolchain build complete. Target runtime tests remain separate.'

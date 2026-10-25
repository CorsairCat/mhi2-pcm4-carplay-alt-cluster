#!/bin/bash
# Run on home after the private JXE conversion in hmi-tools. No installation.
set -euo pipefail
workspace="$HOME/dev/pcm4-qnx-toolchain"
cd "$workspace/hmi-tools"
expected_jxe=1eccb000da3ef57e35dec0d59e4b0be5df375b20b41457fcd53f9bca75307a88
printf '%s  pcm4-lsd.jxe\n' "$expected_jxe" | sha256sum -c -
[ "$(git -C jxe2jar rev-parse HEAD)" = 9eeb45bbf14bf8afe3452c7be96a4d1f0206a286 ]
sha256sum -c <<'HASHES'
adf46d5e34940bdf148ecdd26a9ee8eea94496a72034ff7141066b3eea5c4e9d  deps/asm-9.7.jar
62f4b3bc436045c1acb5c3ba2d8ec556ec3369093d7f5d06c747eb04b56d52b1  deps/asm-tree-9.7.jar
7bc6bcbc21379948a0c8c467fb0f864206e5b818f6bc0b546872f5c9f941556f  deps/asm-analysis-9.7.jar
HASHES
mkdir -p builds
run_dir=$(mktemp -d "$PWD/builds/bridge.XXXXXXXX")
image=eclipse-temurin@sha256:185d4ef919b98cd195690c33f3fd93bd49c66c70f0b662e54566c0dbaebea6ce
sudo -n docker run --rm --network none --read-only --cap-drop ALL \
    --security-opt no-new-privileges --user "$(id -u):$(id -g)" \
    --tmpfs /tmp:rw,nosuid,nodev,size=128m \
    --mount "type=bind,src=$PWD,dst=/work,readonly" \
    --mount "type=bind,src=$workspace/pcm4-project/hmi,dst=/source,readonly" \
    --mount "type=bind,src=$run_dir,dst=/out" \
    -w /work "$image" bash -c '
set -euo pipefail
mkdir -p /out/classes /out/tools /out/tests /out/runtime
stock=/work/pcm4-stock.jar
osgi=/work/jxe2jar/libs/org.osgi.framework-1.10.0.jar
asm=/work/deps/asm-9.7.jar:/work/deps/asm-tree-9.7.jar:/work/deps/asm-analysis-9.7.jar
javac -source 1.4 -target 1.4 -Xlint:-options -cp "$stock:$osgi" \
    -d /out/classes /source/src/local/pcm4/ProjectionBridge.java /source/src/local/pcm4/LoadProbe.java
javac -cp "$asm" -d /out/tools /source/tools/PatchPorsche.java
java -Xverify:all -cp "/out/tools:$asm" PatchPorsche "$stock" /out/classes /out/pcm4-cluster-addon.jar
javac -cp "/out/pcm4-cluster-addon.jar:$stock:$osgi" -d /out/tests /source/tests/BridgeTest.java /source/tests/DisabledBridgeTest.java /source/tests/ContextCollisionTest.java
java -Xverify:all -cp "/out/tests:/out/pcm4-cluster-addon.jar:$stock:$osgi" DisabledBridgeTest
java -Xverify:all -cp "/out/tests:/out/pcm4-cluster-addon.jar:$stock:$osgi" ContextCollisionTest
java -Xverify:all -cp "/out/pcm4-cluster-addon.jar:$stock:$osgi" local.pcm4.LoadProbe
java -Xverify:all -cp "/out/tests:/out/pcm4-cluster-addon.jar:$stock:$osgi" BridgeTest /out/runtime
javap -verbose -classpath /out/pcm4-cluster-addon.jar local.pcm4.ProjectionBridge > /out/bridge-class.txt
javap -c -p -classpath /out/pcm4-cluster-addon.jar de.audi.tghu.fwhmi.porsche.DisplayManagerMIB2PorscheHigh > /out/patched-porsche.txt
'
sha256sum "$run_dir/pcm4-cluster-addon.jar" >"$run_dir/SHA256SUMS"
printf '%s\n' "$run_dir" >latest-build.txt
echo "Offline HMI build and tests complete: $run_dir"

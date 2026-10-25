#!/bin/sh
# Explicit future on-unit controls; no reboot or service killing.
set -eu
base=/mnt/app/root/pcm4-projection
case "${1-}" in
  disable)
    : > /tmp/pcm4-cluster-disable
    rm -f "$base/enabled"
    echo 'Java release requested; hooks disabled for next process start. Restart MMI manually.'
    ;;
  enable)
    [ -f "$base/libpcm4-carplay.so" ] || { echo 'Integration payload missing.' >&2; exit 1; }
    rm -f /tmp/pcm4-cluster-disable
    : > "$base/enabled"
    echo 'Candidate enabled for next startup. Start renderer and restart MMI manually.'
    ;;
  renderer)
    export IPL_CONFIG_DIR=/etc/eso/production
    exec "$base/pcm4-projection-runtime" --enable /armle/lib/libnvss_video.so
    ;;
  probe)
    # Native /ifs JXE remains the factory class source. The original startup
    # discovers our addon JAR before that image, matching its jars directory path.
    exec /ifs/jre/bin/j9 -jcl:foun11 \
      -Xbootclasspath:/mnt/app/eso/hmi/lsd/jars/pcm4-cluster-addon.jar:/ifs/lsd.jxe \
      local.pcm4.LoadProbe
    ;;
  *) echo "usage: $0 enable|disable|renderer|probe" >&2; exit 2 ;;
esac

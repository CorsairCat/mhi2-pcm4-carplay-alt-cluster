#!/bin/sh
# Optional future on-unit collection, invoked manually from a known shell.
# Queries only: never declares/switches contexts or enables debug rendering.
# stdout is the evidence artifact; redirect it to an owner-selected location.
set -u
if [ "${1-}" != "--queries-only" ] || [ "$#" -ne 1 ]; then
    echo "usage: sh capture_pcm4_display_state.sh --queries-only" >&2
    exit 2
fi
if [ "$(uname -s)" != QNX ]; then
    echo "This collector requires a QNX unit; host execution stopped." >&2
    exit 2
fi
export IPL_CONFIG_DIR=/etc/eso/production
printf '\nPCM4 display evidence: '
date
uname -a
for item in /var/dm_force_res /var/dm_force_res_aspect; do
    if [ -e "$item" ]; then printf 'present: %s\n' "$item";
    else printf 'absent: %s\n' "$item"; fi
done
if [ ! -x /eso/bin/apps/dmdt ]; then
    echo "dmdt unavailable" >&2
    exit 1
fi
failed=0
for query in gs gd gc; do
    printf '\ndmdt %s\n' "$query"
    /eso/bin/apps/dmdt "$query" || failed=1
done
exit "$failed"

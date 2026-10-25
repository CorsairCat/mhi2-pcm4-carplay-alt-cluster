#!/bin/sh
# Run from a removable-media folder using an already available QNX shell.
# Reads system files; writes only to a NEW directory chosen by the owner.
set -eu
if [ "$#" -ne 1 ]; then
    echo 'usage: sh collect.sh /absolute/removable-media/new-results-folder' >&2
    exit 2
fi
[ "$(uname -s)" = QNX ] || { echo 'QNX required' >&2; exit 2; }
case "$1" in /*) ;; *) echo 'Absolute destination required' >&2; exit 2 ;; esac
mkdir "$1" # deliberately refuse to overwrite an existing collection
out=$1
exec >"$out/collection.txt" 2>&1
date
uname -a
export IPL_CONFIG_DIR=/etc/eso/production
if [ -x /eso/bin/apps/dmdt ]; then
    for query in gs gd gc; do
        printf '\ndmdt %s\n' "$query"
        /eso/bin/apps/dmdt "$query" || echo "QUERY FAILED: $query"
    done
else
    echo 'MISSING: /eso/bin/apps/dmdt'
fi
for item in /var/dm_force_res /var/dm_force_res_aspect; do
    if [ -e "$item" ]; then echo "PRESENT: $item"; else echo "ABSENT: $item"; fi
done
# Preserve candidate libraries for HOST-side hashes. Record every found path:
# mounted app layout is not assumed to equal the extracted filesystem layout.
mkdir "$out/libraries"
for name in libnvss_video.so libKD.so libnvmedia.so libnvtvmr.so libnvdc.so libairplay.so; do
    found=0
    for base in /armle/lib /armle/usr/lib /lib /usr/lib /eso/lib /mnt/app/armle/lib /mnt/app/eso/lib; do
        if [ -f "$base/$name" ]; then
            id=$(echo "$base" | tr '/' '_')
            cp "$base/$name" "$out/libraries/${id}__${name}"
            echo "COPIED: $base/$name -> libraries/${id}__${name}"
            found=1
        fi
    done
    [ "$found" -eq 1 ] || echo "NOT FOUND IN CANDIDATE PATHS: $name"
done
echo 'Collection complete. Return this directory privately; do not commit vehicle libraries.'

#!/bin/sh
# Prepared candidate: explicit installation only; no mounts/reboots/service kills.
set -eu
cd "$(dirname "$0")"
base=$PWD
prefix=${PCM4_INSTALL_ROOT-}
if [ "$(uname -s)" != QNX ]; then
    [ "${PCM4_INSTALL_TEST-}" = 1 ] && [ -n "$prefix" ] || { echo 'Requires QNX; host tests need an isolated root.' >&2; exit 2; }
else
    [ -z "$prefix" ] || { echo 'Alternate root is a host-test option only.' >&2; exit 2; }
fi
verify=$base/pcm4-verify
check_payload() {
    [ ! -e "$prefix/mnt/app/root/pcm4-projection/enabled" ] || {
        echo 'Existing activation flag: disable the previous candidate before installation.' >&2; return 1;
    }
    while IFS='|' read -r source target mode expected original; do
        [ -n "$source" ] || continue
        "$verify" "$base/$source" "$expected"
        case "$target" in /mnt/app/*) ;; *) echo 'Invalid target.' >&2; return 1 ;; esac
        if [ "$original" = NEW ]; then
            [ ! -e "$prefix$target" ] || { echo "Target already exists: $target" >&2; return 1; }
        else
            "$verify" "$prefix$target" "$original"
        fi
    done < manifest
}
restore_files() {
    directory=$1
    rm -f "$prefix/mnt/app/root/pcm4-projection/enabled"
    : > "$prefix/tmp/pcm4-cluster-disable"
    while IFS='|' read -r source target mode expected original; do
        [ -n "$source" ] || continue
        rm -f "$prefix$target.pcm4-new-$$"
        if [ "$original" = NEW ]; then
            rm -f "$prefix$target"
        else
            cp -p "$directory/$source.original" "$prefix$target"
        fi
    done < "$directory/manifest"
}
case "${1-}" in
  --check)
    [ "$#" = 1 ] || exit 2
    check_payload
    echo 'PASS: payload and original-file identities; no changes applied.'
    ;;
  --install)
    [ "$#" = 2 ] || { echo 'usage: install.sh --install /absolute/NEW-backup-directory' >&2; exit 2; }
    case "$2" in /*) ;; *) exit 2 ;; esac
    check_payload
    [ ! -e "$2" ] || { echo 'Backup directory must be new.' >&2; exit 2; }
    mkdir "$2"
    backup=$2
    cp manifest "$backup/manifest"
    while IFS='|' read -r source target mode expected original; do
        [ "$original" = NEW ] || cp -p "$prefix$target" "$backup/$source.original"
    done < manifest
    # Snapshot every original before publishing any replacement.
    trap 'restore_files "$backup"; echo "Installation failed; originals restored from $backup" >&2' EXIT HUP INT TERM
    while IFS='|' read -r source target mode expected original; do
        directory=$(dirname "$prefix$target")
        mkdir -p "$directory"
        candidate="$prefix$target.pcm4-new-$$"
        cp "$base/$source" "$candidate"
        chmod "$mode" "$candidate"
        "$verify" "$candidate" "$expected"
        mv "$candidate" "$prefix$target"
    done < manifest
    trap - EXIT HUP INT TERM
    echo "Installed disabled candidate. Backup: $backup"
    echo 'Perform the documented loading tests before enabling. No restart requested.'
    ;;
  --restore)
    [ "$#" = 2 ] || exit 2
    backup=$2
    [ -f "$backup/manifest" ] || exit 2
    # Do not overwrite changes made after this installation.
    while IFS='|' read -r source target mode expected original; do
        case "$target" in /mnt/app/*) ;; *) exit 2 ;; esac
        "$verify" "$prefix$target" "$expected"
        [ "$original" = NEW ] || "$verify" "$backup/$source.original" "$original"
    done < "$backup/manifest"
    restore_files "$backup"
    echo 'Prior files restored. Restart MMI manually; backup retained.'
    ;;
  *) echo 'usage: install.sh --check | --install NEW_BACKUP | --restore BACKUP' >&2; exit 2 ;;
esac

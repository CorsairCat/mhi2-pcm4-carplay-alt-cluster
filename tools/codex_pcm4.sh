#!/bin/sh
# Project entry point; the running parent chat is unaffected.
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec codex -C "$project_dir" -c model_auto_compact_token_limit=330000 "$@"

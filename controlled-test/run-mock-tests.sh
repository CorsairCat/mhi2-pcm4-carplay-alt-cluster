#!/bin/sh
# These programs use only original in-memory/synthetic NvSS implementations.
# They do not load vehicle NvSS or request any physical display output.
set -eu
[ "$(uname -s)" = QNX ] || { echo 'Run this stage on the QNX unit.' >&2; exit 2; }
cd "$(dirname "$0")"
base=$PWD/bin/qnx-mocks
"$base/display-mock-test"
"$base/projection-mock-test"
"$base/hook-mock-test"
"$base/binding-mock-test" "$base/mock.so" "$base/missing.so"
echo 'PASS: QNX mock stages. No real video decode, HMI switch or display tested.'

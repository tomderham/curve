#!/bin/sh
# Builds and runs the Meter node's measurement tests, sanitized and at -O2.
set -e

here="$(cd "$(dirname "$0")" && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

clang++ -std=c++17 -O2 -Wall -Wextra -fsanitize=address,undefined "$here/test_meter_dsp.cpp" -o "$out/test_meter_dsp"
"$out/test_meter_dsp"

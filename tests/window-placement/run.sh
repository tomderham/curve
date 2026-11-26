#!/bin/sh
# Builds and runs the plug-in window placement tests, sanitized.
set -e

here="$(cd "$(dirname "$0")" && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

clang++ -std=c++17 -O2 -Wall -Wextra -fsanitize=address,undefined "$here/test_window_placement.cpp" -o "$out/test_window_placement"
"$out/test_window_placement"

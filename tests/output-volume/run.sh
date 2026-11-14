#!/bin/sh
# Builds and runs the output volume curve and step tests, sanitized.
set -e

here="$(cd "$(dirname "$0")" && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

clang++ -std=c++17 -O2 -Wall -Wextra -fsanitize=address,undefined "$here/test_output_volume.cpp" -o "$out/test_output_volume"
"$out/test_output_volume"

#!/bin/sh
# Builds and runs the output monitor tests, sanitized.
set -e

here="$(cd "$(dirname "$0")" && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

clang++ -std=c++17 -O2 -Wall -Wextra -fsanitize=address,undefined "$here/test_output_monitor.cpp" -o "$out/test_output_monitor"
"$out/test_output_monitor"

#!/bin/sh
# Builds and runs the preset key and program number tests, sanitized.
set -e

here="$(cd "$(dirname "$0")" && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

clang++ -std=c++17 -O2 -Wall -Wextra -fsanitize=address,undefined -I "$here/../../Source/UI" "$here/test_preset_slots.cpp" -o "$out/test_preset_slots"
"$out/test_preset_slots"

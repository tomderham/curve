#!/bin/sh
# Builds and runs the plug-in panel plug-in list tests, sanitized.
set -e

here="$(cd "$(dirname "$0")" && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

clang++ -std=c++17 -O2 -Wall -Wextra -fsanitize=address,undefined "$here/test_panel_plugins.cpp" -o "$out/test_panel_plugins"
"$out/test_panel_plugins"

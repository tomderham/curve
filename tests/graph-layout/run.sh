#!/bin/sh
# Builds and runs the graph auto-layout tests, sanitized and at -O2.
set -e

here="$(cd "$(dirname "$0")" && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

clang++ -std=c++17 -O2 -Wall -Wextra -fsanitize=address,undefined "$here/test_graph_layout.cpp" -o "$out/test_graph_layout"
"$out/test_graph_layout"

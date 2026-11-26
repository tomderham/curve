#!/bin/sh
# Builds and runs the tests of what Curve does at launch with unsaved changes it kept, sanitized.
set -e

here="$(cd "$(dirname "$0")" && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

clang++ -std=c++17 -O2 -Wall -Wextra -fsanitize=address,undefined -I "$here/../../Source/UI" "$here/test_session_recovery.cpp" -o "$out/test_session_recovery"
"$out/test_session_recovery"

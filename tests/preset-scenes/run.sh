#!/bin/sh
# Builds and runs the preset scene tests against juce_core, sanitized.
set -e

here="$(cd "$(dirname "$0")" && pwd)"
juce="$here/../../JUCE/modules"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

defines="-DDEBUG=1 -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_STANDALONE_APPLICATION=1 -DJUCE_USE_CURL=0 -DJUCE_WEB_BROWSER=0"

clang++ -std=c++17 -O1 -fsanitize=address,undefined $defines -I"$juce" -w -c "$juce/juce_core/juce_core.mm" -o "$out/juce_core.o"
clang++ -std=c++17 -O1 $defines -I"$juce" -c "$juce/juce_core/juce_core_CompilationTime.cpp" -o "$out/juce_time.o"
clang++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined $defines -I"$juce" -c "$here/test_preset_scenes.cpp" -o "$out/test.o"
clang++ -fsanitize=address,undefined "$out/test.o" "$out/juce_core.o" "$out/juce_time.o" -framework Foundation -framework AppKit -framework IOKit -framework Security -framework CoreServices -o "$out/test_preset_scenes"
"$out/test_preset_scenes"

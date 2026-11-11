#!/bin/sh
# Usage: tests/run_host.sh [folder for WAV renders]
set -eu
cd "$(dirname "$0")/.."
test_build=$(mktemp -d /private/tmp/talko-drums-host.XXXXXX)
trap 'rm -rf "$test_build"' EXIT
sanitize="-fsanitize=address,undefined -fno-sanitize-recover=all"
cc -O2 -g -std=c11 -Wall -Wextra -Werror -Wno-unused-function $sanitize \
  -c src/drum_engine.c -o "$test_build/drum_engine.o"
# Hortator's sources are kept as published: their warnings are not ours to fix. They shift
# negative values left, which GCC defines as two's complement (and C23 makes standard).
cc -O2 -g -std=c11 -w -DDM_QCHECK $sanitize -fno-sanitize=shift-base \
  -c src/hortator_engine.c -o "$test_build/hortator_engine.o"
c++ -O2 -g -std=c++17 -Wall -Wextra -Werror $sanitize \
  tests/drums.cpp src/effects.cpp "$test_build/drum_engine.o" "$test_build/hortator_engine.o" \
  -o "$test_build/drums"
c++ -O2 -g -std=c++17 -Wall -Wextra -Werror $sanitize \
  tests/hortator_presets.cpp "$test_build/hortator_engine.o" -o "$test_build/hortator_presets"
if [ $# -gt 0 ]; then
  mkdir -p "$1/hortator"
  "$test_build/hortator_presets" "$1/hortator" > "$1/hortator/presets.txt"
else
  "$test_build/hortator_presets" > /dev/null
fi
"$test_build/drums" "$@"

#!/bin/sh
# Usage: tests/run_host.sh [folder for WAV renders]
set -eu
cd "$(dirname "$0")/.."
test_build=$(mktemp -d /private/tmp/talko-drums-host.XXXXXX)
trap 'rm -rf "$test_build"' EXIT
sanitize="-fsanitize=address,undefined -fno-sanitize-recover=all"
cc -O2 -g -std=c11 -Wall -Wextra -Werror -Wno-unused-function $sanitize \
  -c src/drum_engine.c -o "$test_build/drum_engine.o"
c++ -O2 -g -std=c++17 -Wall -Wextra -Werror $sanitize \
  tests/drums.cpp src/effects.cpp "$test_build/drum_engine.o" -o "$test_build/drums"
if [ $# -gt 0 ]; then
  mkdir -p "$1"
fi
"$test_build/drums" "$@"

#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread -DGIT_UPDATE_TRANSFER_CLOCK_TEST \
  -I"$json" -I"$repo/src" "$repo/tests/manual-update-clock/test.cpp" "$repo/src/GitUpdate.cpp" -o "$scratch/test"
"$scratch/test"

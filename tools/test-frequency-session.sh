#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -DESPSOMFY_LOCAL_UI -I"$json" -I"$repo/tests/frequency-session-runtime" -I"$repo/test/stubs" -I"$repo/src" \
 "$repo/tests/frequency-session-runtime/runtime-test.cpp" "$repo/src/GitUpdate.cpp" "$repo/src/SomfyFrame.cpp" -pthread -o "$scratch/session"
"$scratch/session"

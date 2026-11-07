#!/usr/bin/env bash
# Host only: scripted HTTPS/FreeRTOS/OTA SDK boundaries; no controller access.
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
compiler="${CXX:-g++}"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
[[ -d "$json" ]] || { echo 'Run pio test -e native first to install pinned ArduinoJson.' >&2;exit 1; }
build="$(mktemp -d)"
trap 'rm -rf -- "$build"' EXIT
"$compiler" -std=c++17 -Wall -Wextra -Werror -I"$json" -I"$repo/src" "$repo/tests/git-update-worker/worker-test.cpp" "$repo/src/GitUpdatePolicy.cpp" -Wl,--wrap=time -l:libcrypto.so.3 -o "$build/worker-test"
"$build/worker-test"

#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_DIR:-$repo/.pio/libdeps/native/ArduinoJson/src}"
[[ -f "$json/ArduinoJson.h" ]] || { echo 'Run native tests first or set ARDUINOJSON_DIR.' >&2; exit 1; }
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -DESPSOMFY_LOCAL_UI \
  -I"$repo/tests/settings-memory" -I"$repo/src" -I"$json" \
  "$repo/tests/settings-memory/memory-test.cpp" "$repo/src/LocalSettingsMemory.cpp" -o "$scratch/memory"
"$scratch/memory"
printf 'Settings PSRAM allocation checks passed\n'

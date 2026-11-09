#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -O2 -pthread -I"$json" -I"$repo/test/stubs" -I"$repo/src" \
  "$repo/tests/legacy-restart/restart-test.cpp" "$repo/src/GitUpdate.cpp" "$repo/src/SettingsDurability.cpp" -o "$scratch/restart"
for case in allowed counter manual expired handoff record modelFailed modelUncertain settings lateSettings lateHistory queue moving pulse groupMoving radio queueSealRace radioSealRace; do "$scratch/restart" "$case"; done

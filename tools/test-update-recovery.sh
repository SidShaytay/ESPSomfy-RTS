#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)";trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -O2 -pthread -DBOOT_RECOVERY_HOST_TEST -I"$json" -I"$repo/test/stubs" -I"$repo/src" \
 "$repo/tests/safe-restart/runtime-test.cpp" "$repo/src/RestartCoordinator.cpp" "$repo/src/RestartPolicy.cpp" \
 "$repo/src/GitUpdate.cpp" "$repo/src/SettingsDurability.cpp" "$repo/src/SomfyFrame.cpp" -o "$scratch/test"
for case in recoveryValid recoveryMovement recoveryRX recoveryWork recoveryStorage recoveryUnavailable recoveryNewMovement recoveryLateRX;do "$scratch/test" "$case";done
printf 'Automatic checked rollback: motion/work/storage waiting, cancellation and late RX passed\n'

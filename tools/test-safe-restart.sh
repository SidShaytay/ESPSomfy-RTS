#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -O2 -pthread -I"$json" -I"$repo/test/stubs" -I"$repo/src" \
 "$repo/tests/safe-restart/runtime-test.cpp" "$repo/src/RestartCoordinator.cpp" "$repo/src/RestartPolicy.cpp" \
 "$repo/src/GitUpdate.cpp" "$repo/src/SettingsDurability.cpp" "$repo/src/SomfyFrame.cpp" -o "$scratch/restart"
for case in allowed clockWrap dirty initialFailure cancelIO counter settings moving held pulse groupUnknown queue radio finalFailure lateSettings lateHistory cancelFinal queueSealRace radioSealRace operation failureRetry revision rollback asyncStale asyncAge ownedCancel ownedExpired ownedHandoff claimed;do "$scratch/restart" "$case";done
printf 'Actual SafeRestart service: history, source, two-phase I/O and final barriers passed\n'

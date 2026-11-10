#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread -I"$json" -I"$repo/tests/application-filesystem" -I"$repo/tests/update-certificate" -I"$repo/test/stubs" -I"$repo/src" \
 "$repo/tests/application-filesystem/runtime-test.cpp" "$repo/src/GitUpdate.cpp" "$repo/src/ShadeMotion.cpp" "$repo/src/SomfyFrame.cpp" -o "$scratch/test"
for case in ready sdk abortBusy foreignSdk expiredReady littlefs layout size save busy moving security unmount expired mount radio recoverySave;do "$scratch/test" "$case";done
echo "Application filesystem: real transfer owner, partition identity, reader drain, verified unmount, no-format remount, radio recovery and current RAM save passed."

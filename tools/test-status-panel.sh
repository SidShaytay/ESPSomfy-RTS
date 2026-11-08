#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
for board in ESPSOMFY_LILYGO_T_EMBED_CC1101; do
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -DESPSOMFY_STATUS_DISPLAY -D"$board" -DCONFIG_IDF_TARGET_ESP32S3 \
  -I"$repo/tests/status-panel" -I"$repo/src" \
  "$repo/tests/status-panel/panel-test.cpp" "$repo/src/StatusDisplay.cpp" -o "$scratch/panel-test"
"$scratch/panel-test"
done
"${CXX:-c++}" -std=c++17 -DESP32 -I"$repo/tests/radio-spi" \
  -I"$repo/lib/SmartRC-CC1101-Driver-Lib" \
  "$repo/tests/radio-spi/radio-test.cpp" \
  "${RADIO_DRIVER_SOURCE:-$repo/lib/SmartRC-CC1101-Driver-Lib/ELECHOUSE_CC1101_SRC_DRV.cpp}" \
  -o "$scratch/radio-test"
"$scratch/radio-test"

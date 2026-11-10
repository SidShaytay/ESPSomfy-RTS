#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
for ethernet in 0 1; do
"${CXX:-c++}" -std=c++17 -DHAS_ETHERNET=$ethernet -Wall -Wextra -Werror -DESPSOMFY_STATUS_DISPLAY -DESPSOMFY_LOCAL_UI \
  -DESPSOMFY_LILYGO_T_EMBED_CC1101 -DCONFIG_IDF_TARGET_ESP32S3 '-DFW_VERSION="health-test"' \
  -I"$repo/tests/status-health" -I"$repo/src" \
  "$repo/tests/status-health/adapter-test.cpp" "$repo/src/BatteryGauge.cpp" -o "$scratch/test"
"$scratch/test"
done
printf 'Production status adapter health: capability counters, zero/missing states and read-only sampling passed\n'

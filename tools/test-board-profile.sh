#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
compiler="${CXX:-c++}"
common=(-std=c++11 -Wall -Wextra -Werror -I"$repo/tests/board-profile" -I"$repo/src")
sources=("$repo/tests/board-profile/board-profile-test.cpp" "$repo/src/BoardProfile.cpp")
"$compiler" "${common[@]}" -DESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS -DCONFIG_IDF_TARGET_ESP32S3 \
  "${sources[@]}" -o "$scratch/lilygo-test"
"$scratch/lilygo-test"
"$compiler" "${common[@]}" -DESPSOMFY_LILYGO_T_EMBED_CC1101 -DCONFIG_IDF_TARGET_ESP32S3 -DEXPECT_INTEGRATED_RADIO \
  "${sources[@]}" -o "$scratch/lilygo-standard-test"
"$scratch/lilygo-standard-test"
"$compiler" "${common[@]}" "${sources[@]}" -o "$scratch/generic-test"
"$scratch/generic-test"
"$compiler" "${common[@]}" -DESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS -DCONFIG_IDF_TARGET_ESP32S3 \
  "$repo/tests/board-profile/shade-gpio-test.cpp" "$repo/src/BoardProfile.cpp" -o "$scratch/shade-gpio-test"
"$scratch/shade-gpio-test"
if "$compiler" "${common[@]}" -DESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS \
  "${sources[@]}" -o "$scratch/wrong-target" > "$scratch/wrong-target.log" 2>&1; then
  echo 'LILYGO profile incorrectly accepted a non-S3 target' >&2
  exit 1
fi
echo 'Non-S3 LILYGO build rejected as expected'

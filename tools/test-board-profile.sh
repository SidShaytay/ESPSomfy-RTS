#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
compiler="${CXX:-c++}"
common=(-std=c++11 -Wall -Wextra -Werror -I"$repo/tests/board-profile" -I"$repo/src")
sources=("$repo/tests/board-profile/board-profile-test.cpp" "$repo/src/BoardProfile.cpp")
for board in ESPSOMFY_LILYGO_T_EMBED_CC1101; do
  "$compiler" "${common[@]}" -D"$board" -DCONFIG_IDF_TARGET_ESP32S3 "${sources[@]}" -o "$scratch/board-test"
  "$scratch/board-test"
  "$compiler" "${common[@]}" -D"$board" -DCONFIG_IDF_TARGET_ESP32S3 -DESPSOMFY_LOCAL_UI "${sources[@]}" -o "$scratch/ui-build-test"
  "$scratch/ui-build-test"
  "$compiler" "${common[@]}" -D"$board" -DCONFIG_IDF_TARGET_ESP32S3 \
    "$repo/tests/board-profile/shade-gpio-test.cpp" "$repo/src/ModelEditor.cpp" "$repo/src/BoardProfile.cpp" -o "$scratch/shade-gpio-test"
  "$scratch/shade-gpio-test"
done
"$compiler" "${common[@]}" "${sources[@]}" -o "$scratch/generic-test"
"$scratch/generic-test"
# Compile exactly the same production feature checks as SomfyController.cpp.
printf '#include "LilygoBuild.h"\nint main() {}\n' > "$scratch/features.cpp"
features=(-DESPSOMFY_STATUS_DISPLAY -DESPSOMFY_LOCAL_UI -DESPSOMFY_LOCAL_COMMANDS)
reject() {
  if "$compiler" "${common[@]}" "$@" "$scratch/features.cpp" -o "$scratch/rejected" > "$scratch/rejected.log" 2>&1; then
    echo "Invalid LILYGO configuration accepted: $*" >&2
    exit 1
  fi
}
for board in ESPSOMFY_LILYGO_T_EMBED_CC1101; do
  "$compiler" "${common[@]}" -D"$board" -DCONFIG_IDF_TARGET_ESP32S3 "${features[@]}" "$scratch/features.cpp" -o "$scratch/features"
  reject -D"$board" "${features[@]}"
  for removed in ESPSOMFY_STATUS_DISPLAY ESPSOMFY_LOCAL_UI ESPSOMFY_LOCAL_COMMANDS; do
    reject -D"$board" -DCONFIG_IDF_TARGET_ESP32S3 "${features[@]}" -U"$removed"
    reject -D"$board" -DCONFIG_IDF_TARGET_ESP32S3 "${features[@]}" -U"$removed" -D"$removed"=0
    reject -D"$board" -DCONFIG_IDF_TARGET_ESP32S3 "${features[@]}" -U"$removed" -D"$removed"=2
  done
  reject -D"$board"=0 -DCONFIG_IDF_TARGET_ESP32S3 "${features[@]}"
done
reject -DESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS -DCONFIG_IDF_TARGET_ESP32S3 "${features[@]}"
reject -DESPSOMFY_LILYGO_T_EMBED_CC1101 -DESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS -DCONFIG_IDF_TARGET_ESP32S3 "${features[@]}"
reject -DCONFIG_IDF_TARGET_ESP32S3 "${features[@]}"
echo 'Missing features, wrong targets and conflicting boards rejected as expected'

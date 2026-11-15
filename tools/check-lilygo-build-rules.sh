#!/usr/bin/env bash
# Compile-checks the feature rules in src/LilygoBuild.h: the LILYGO board needs
# display, local UI, local commands and bundled web, and generic boards must
# not set them.  These rules are #error directives, so they can only be checked
# by compiling; the board's pins and radio logic are in the native tests
# (pio test -e native).  Needs a host C++ compiler: CXX=g++ bash tools/check-lilygo-build-rules.sh
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
compiler="${CXX:-c++}"
# Arduino.h is only a stub here; nothing but the preprocessor rules is checked.
common=(-std=c++17 -fsyntax-only -I"$repo/test/stubs" -I"$repo/src")
printf '#include "LilygoBuild.h"\nint main() {}\n' > "$scratch/features.cpp"
features=(-DESPSOMFY_STATUS_DISPLAY=1 -DESPSOMFY_LOCAL_UI=1 -DESPSOMFY_LOCAL_COMMANDS=1 -DESPSOMFY_BUNDLED_WEB=1)
board=(-DESPSOMFY_LILYGO_T_EMBED_CC1101=1 -DCONFIG_IDF_TARGET_ESP32S3=1)
accept() {
  "$compiler" "${common[@]}" "$@" "$scratch/features.cpp" || { echo "Valid configuration rejected: $*" >&2; exit 1; }
}
reject() {
  if "$compiler" "${common[@]}" "$@" "$scratch/features.cpp" > "$scratch/rejected.log" 2>&1; then
    echo "Invalid LILYGO configuration accepted: $*" >&2
    exit 1
  fi
}
accept "${board[@]}" "${features[@]}"
accept # a generic board with no local features
for removed in ESPSOMFY_STATUS_DISPLAY ESPSOMFY_LOCAL_UI ESPSOMFY_LOCAL_COMMANDS ESPSOMFY_BUNDLED_WEB; do
  reject "${board[@]}" "${features[@]}" -U"$removed"
  reject "${board[@]}" "${features[@]}" -U"$removed" -D"$removed"=0
  reject "${board[@]}" "${features[@]}" -U"$removed" -D"$removed"=2
  # A generic board must not take a local feature either.
  reject -D"$removed"=1
done
reject -DESPSOMFY_LILYGO_T_EMBED_CC1101=0 -DCONFIG_IDF_TARGET_ESP32S3=1 "${features[@]}"
reject -DESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS=1 -DCONFIG_IDF_TARGET_ESP32S3=1 "${features[@]}"
reject -DESPSOMFY_LILYGO_T_EMBED_CC1101=1 -DESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS=1 -DCONFIG_IDF_TARGET_ESP32S3=1 "${features[@]}"
reject -DESPSOMFY_LILYGO_T_EMBED_CC1101=1 "${features[@]}" # not an ESP32-S3
echo 'LILYGO feature rules accept the full set and reject every missing, wrong-target or conflicting configuration'

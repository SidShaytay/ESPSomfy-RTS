#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
gfx="${GFX_DIR:-$repo/.pio/libdeps/lilygo-t-embed-cc1101-plus-ui/Adafruit GFX Library}"
out="${1:-$repo/build/ui-preview}"
[[ -f "$gfx/Adafruit_GFX.cpp" ]] || { echo 'Build the UI env first, or set GFX_DIR to its pinned Adafruit GFX library directory.' >&2; exit 1; }
mkdir -p "$out"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -O2 -DARDUINO=10800 -DLOCAL_UI_HOST_PREVIEW \
  -I"$repo/tests/ui-render" -I"$repo/src" -I"$gfx" \
  "$repo/tests/ui-render/render.cpp" "$repo/src/LocalUiRenderer.cpp" "$repo/src/StatusDisplay.cpp" "$gfx/Adafruit_GFX.cpp" \
  -o "$scratch/render"
"$scratch/render" "$out"
"${CXX:-c++}" -std=c++17 -O2 -DARDUINO=10800 -DESPSOMFY_LOCAL_UI -DUI_RUNTIME_HOST_TEST \
  -DESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS -DCONFIG_IDF_TARGET_ESP32S3 \
  -I"$repo/tests/ui-render" -I"$repo/src" -I"$gfx" \
  "$repo/tests/ui-render/runtime-test.cpp" "$repo/src/LocalUiRuntime.cpp" "$repo/src/LocalUiRenderer.cpp" \
  "$repo/src/LocalUi.cpp" "$repo/src/StatusDisplay.cpp" "$gfx/Adafruit_GFX.cpp" \
  -o "$scratch/runtime"
"$scratch/runtime"
"${CXX:-c++}" -std=c++17 -O2 -DESPSOMFY_LOCAL_UI -DUI_RUNTIME_HOST_TEST \
  -I"$repo/tests/ui-render" -I"$repo/src" \
  "$repo/tests/ui-render/capture-test.cpp" "$repo/src/LocalUiCapture.cpp" -o "$scratch/capture"
"$scratch/capture"
bash "$repo/tools/test-local-ui-adapter.sh"
python3 "$repo/tools/local_ui_previews.py" "$out"

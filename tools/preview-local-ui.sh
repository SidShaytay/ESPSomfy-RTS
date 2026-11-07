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
python3 "$repo/tools/local_ui_previews.py" "$out"

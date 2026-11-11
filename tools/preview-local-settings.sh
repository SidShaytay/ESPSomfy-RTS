#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
gfx="${GFX_DIR:-$repo/.pio/libdeps/lilygo-t-embed-cc1101/Adafruit GFX Library}"
out="${1:-$repo/build/settings-preview}"
[[ -f "$gfx/Adafruit_GFX.cpp" ]] || { echo 'Build the shared environment first or set GFX_DIR.' >&2; exit 1; }
mkdir -p "$out"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -O2 -DARDUINO=10800 -DLOCAL_UI_HOST_PREVIEW \
  -I"$repo/tests/ui-render" -I"$repo/src" -I"$gfx" \
  "$repo/tests/ui-render/settings-render.cpp" "$repo/src/LocalUiRenderer.cpp" "$repo/src/LocalControlPolicy.cpp" \
  "$repo/src/StatusDisplay.cpp" "$gfx/Adafruit_GFX.cpp" -o "$scratch/render"
"$scratch/render" "$out"
python3 - "$repo" "$out" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0,str(Path(sys.argv[1])/'tools'))
from lcd_image import png_bytes
for path in Path(sys.argv[2]).glob('settings-*.ppm'):
    head=path.read_bytes().split(b'\n',3)
    if head[:3] != [b'P6',b'320 170',b'255']:
        raise ValueError('Unexpected renderer output')
    path.with_suffix('.png').write_bytes(png_bytes(320,170,head[3]))
PY

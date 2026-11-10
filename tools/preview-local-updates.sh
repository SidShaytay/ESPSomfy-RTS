#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
gfx="${GFX_DIR:-$repo/.pio/libdeps/lilygo-t-embed-cc1101/Adafruit GFX Library}"
json="${ARDUINOJSON_DIR:-$repo/.pio/libdeps/native/ArduinoJson/src}"
out="${1:-$repo/build/update-preview}"
mkdir -p "$out"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -O2 -DARDUINO=10800 -DLOCAL_UI_HOST_PREVIEW \
 -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_PROGMEM=0 \
 -I"$json" -I"$repo/tests/ui-render" -I"$repo/tests/ui-adapter" -I"$repo/test/stubs" -I"$repo/src" -I"$gfx" \
 "$repo/tests/ui-render/update-render.cpp" "$repo/src/LocalUpdates.cpp" "$repo/src/LocalControlPolicy.cpp" "$repo/src/LocalUiRenderer.cpp" "$repo/src/StatusDisplay.cpp" "$gfx/Adafruit_GFX.cpp" -o "$scratch/update-render"
"$scratch/update-render" "$out"
python3 - "$repo" "$out" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0,str(Path(sys.argv[1])/'tools'))
from lcd_image import png_bytes
for path in Path(sys.argv[2]).glob('update-*.ppm'):
 head=path.read_bytes().split(b'\n',3)
 assert head[:3]==[b'P6',b'320 170',b'255']
 path.with_suffix('.png').write_bytes(png_bytes(320,170,head[3]))
PY

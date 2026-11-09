#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
# Compile the complete production emitFrame body unchanged. All endpoints here
# are observer/JSON sinks; the executable has no RF/GPIO/NVS/controller endpoint.
python3 - "$repo/src/Transceiver.cpp" "$scratch/production-emit.inc" <<'PY'
from pathlib import Path
import sys
text=Path(sys.argv[1]).read_text();start=text.index('void Transceiver::emitFrame(')
brace=text.index('{',start);end=brace+1;depth=1
while depth:
 depth+=(text[end]=='{')-(text[end]=='}');end+=1
Path(sys.argv[2]).write_text(text[start:end]+'\n')
PY
"${CXX:-c++}" -std=c++17 -O2 -DESPSOMFY_LOCAL_UI -I"$scratch" -I"$json" -I"$repo/test/stubs" -I"$repo/src" \
 "$repo/tests/local-receive/emit-test.cpp" "$repo/src/LocalReceive.cpp" "$repo/src/SomfyFrame.cpp" \
 "$repo/src/ProtoSchema.cpp" "$repo/src/JsonFormatter.cpp" -o "$scratch/emit-test"
"$scratch/emit-test"

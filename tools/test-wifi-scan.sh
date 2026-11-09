#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch" <<'PY'
from pathlib import Path
import sys
repo,scratch=map(Path,sys.argv[1:]);text=(repo/'src/ESPNetwork.cpp').read_text()
start=text.index('static WifiClientScan clientScanner(');end=text.index('void ESPNetwork::end()',start)
(scratch/'production-wifi-scan.inc').write_text(text[start:end])
PY
"${CXX:-c++}" -std=c++17 -O2 -pthread -I"$json" -I"$scratch" -I"$repo/test/stubs" -I"$repo/src" \
 "$repo/tests/wifi-scan/runtime-test.cpp" "$repo/src/WifiClientScan.cpp" "$repo/src/WifiScanFeed.cpp" "$repo/src/GitUpdate.cpp" -o "$scratch/scan"
"$scratch/scan"

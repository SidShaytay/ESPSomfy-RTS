#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch/history.inc" <<'PY'
from pathlib import Path
import sys
text=(Path(sys.argv[1])/'src/DeviceOps.cpp').read_text()
a=text.index('// Synchronized settings history:');b=text.index('// End synchronized settings history.',a)
Path(sys.argv[2]).write_text(text[a:b])
PY
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread -I"$scratch" -I"$repo/test/stubs" -I"$repo/.pio/libdeps/native/ArduinoJson/src" -I"$repo/src" "$repo/tests/settings-history/test.cpp" -o "$scratch/test"
"$scratch/test"

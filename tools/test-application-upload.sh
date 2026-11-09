#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch/restore-options.inc" <<'PYCODE'
from pathlib import Path
import sys
text=(Path(sys.argv[1])/'src/ConfigSettings.cpp').read_text()
start=text.index('void restore_options_t::fromJSON(');brace=text.index('{',start);end=brace+1;depth=1
while depth:
    depth+=(text[end]=='{')-(text[end]=='}');end+=1
Path(sys.argv[2]).write_text(text[start:end]+'\n')
PYCODE
"${CXX:-c++}" -std=c++17 -O2 -I"$scratch" -I"$json" -pthread -I"$repo/tests/application-upload" -I"$repo/tests/ui-adapter" -I"$repo/test/stubs" -I"$repo/src" \
  "$repo/tests/application-upload/application-upload-test.cpp" "$repo/src/GitUpdate.cpp" "$repo/src/ShadeRequests.cpp" "$repo/src/ShadeMotion.cpp" "$repo/src/RestartCoordinator.cpp" "$repo/src/RestartPolicy.cpp" "$repo/src/SettingsDurability.cpp" "$repo/src/SomfyFrame.cpp" -o "$scratch/application-upload"
"$scratch/application-upload"

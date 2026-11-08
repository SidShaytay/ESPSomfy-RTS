#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
# Compile these unchanged reset/identity bodies verbatim with the actual model
# header. Replace only hardware/storage vtable endpoints in the harness.
python3 - "$repo" "$scratch/production-reset.inc" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);out=[]
for path,names in [('SomfyRemote.cpp',['SomfyRemote::setRemoteAddress','SomfyRemote::getRemoteAddress','SomfyRemote::p_lastRollingCode','SomfyLinkedRemote::SomfyLinkedRemote']),('SomfyGroup.cpp',['SomfyGroup::clear']),('SomfyShade.cpp',['SomfyShade::clear','SomfyShade::onChanged','SomfyShade::getFlags','SomfyShade::setFlags','SomfyShade::p_lastRollingCode'])]:
    text=(root/'src'/path).read_text()
    for name in names:
        at=text.index(name+'(');start=text.rfind('\n',0,at)+1;brace=text.index('{',at);end=brace+1;depth=1
        while depth:
            depth+=(text[end]=='{')-(text[end]=='}');end+=1
        out.append(text[start:end])
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
PY
"${CXX:-c++}" -std=c++17 -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -I"$scratch" -I"$json" -I"$repo/test/stubs" -I"$repo/src" \
  "$repo/tests/model-allocation/allocation-test.cpp" "$repo/src/ModelEditor.cpp" \
  "$repo/src/ShadeBehaviour.cpp" "$repo/src/ShadeSensors.cpp" -o "$scratch/allocation"
"$scratch/allocation"

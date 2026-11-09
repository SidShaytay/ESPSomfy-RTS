#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)";trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch/socket.inc" <<'PY'
from pathlib import Path
import sys
text=(Path(sys.argv[1])/'src/outputs/ProtoSocket.cpp').read_text();out=[]
for name in ['beginEvent','finishEvent','ProtoSocket::hasClients','ProtoSocket::sending','ProtoSocket::retireModelSessions','ProtoSocket::modelPersistence']:
 a=text.index(name+'(');start=text.rfind('\n',0,a)+1;b=text.index('{',a);end=b+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 out.append(text[start:end])
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
PY
"${CXX:-c++}" -std=c++17 -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections -I"$scratch" -I"$repo/test/stubs" -I"$repo/src" -I"$json" \
 "$repo/tests/restore-completion/test.cpp" "$repo/src/ProtoSlots.cpp" "$repo/src/ProtoSync.cpp" "$repo/src/ProtoFrame.cpp" "$repo/src/ProtoSchema.cpp" "$repo/src/ProtoModelResults.cpp" "$repo/src/ModelEditor.cpp" "$repo/src/ModelPersistence.cpp" "$repo/src/JsonFormatter.cpp" "$repo/src/ItemSnapshot.cpp" "$repo/src/Utils.cpp" "$repo/src/RestartPolicy.cpp" -o "$scratch/test"
"$scratch/test"

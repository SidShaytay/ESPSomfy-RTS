#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)";trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);out=Path(sys.argv[2])
for file,names,target in [('src/outputs/ProtoSocket.cpp',['queuedMutationOrigin','ProtoSocket::acceptsClientMutation','ProtoSocket::acceptsMutation','ProtoSocket::discardRetiredMutation','ProtoSocket::retireModelSessions','sendRemoteCommand'],'socket.inc'),('src/ShadeController.cpp',['SomfyShadeController::sendFrame'],'controller.inc'),('src/LoopQueue.cpp',['LoopQueue::begin','LoopQueue::submit','LoopQueue::sealForRestart','LoopQueue::reopenAfterRefusedRestart','LoopQueue::pending','LoopQueue::beginSocketMutation','LoopQueue::endSocketMutation','LoopQueue::drain'],'queue.inc')]:
 text=(root/file).read_text();bodies=[]
 for name in names:
  a=text.index(name+'(');start=text.rfind('\n',0,a)+1;b=text.index('{',a);end=b+1;depth=1
  while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
  bodies.append(text[start:end])
 (out/target).write_text('\n'.join(bodies)+'\n')
PY
"${CXX:-c++}" -std=c++17 -O2 -pthread -I"$scratch" -I"$json" -I"$repo/tests/ui-adapter" -I"$repo/test/stubs" -I"$repo/src" -I"$json" \
 "$repo/tests/socket-retirement/test.cpp" "$repo/src/ProtoSlots.cpp" "$repo/src/ShadeRequests.cpp" "$repo/src/ShadeMotion.cpp" "$repo/src/TxBuffer.cpp" "$repo/src/SomfyFrame.cpp" "$repo/src/ProtoSync.cpp" "$repo/src/ProtoFrame.cpp" "$repo/src/ProtoSchema.cpp" "$repo/src/JsonFormatter.cpp" "$repo/src/ItemSnapshot.cpp" "$repo/src/Utils.cpp" -o "$scratch/test"
"$scratch/test"

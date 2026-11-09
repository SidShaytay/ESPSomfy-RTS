#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch/production-queue.inc" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);text=(root/'src/LoopQueue.cpp').read_text();out=[]
for name in ['LoopQueue::begin','LoopQueue::submit','LoopQueue::pending','LoopQueue::sealForRestart','LoopQueue::reopenAfterRefusedRestart']:
 at=text.index(name+'(');start=text.rfind('\n',0,at)+1;brace=text.index('{',at);end=brace+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 out.append(text[start:end])
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
PY
"${CXX:-c++}" -std=c++17 -O2 -pthread -I"$scratch" -I"$repo/tests/restore-execution" -I"$json" -I"$repo/tests/ui-adapter" -I"$repo/test/stubs" -I"$repo/src" \
 "$repo/tests/restore-execution/runtime-test.cpp" "$repo/src/SomfyFrame.cpp" -o "$scratch/restore"
for case in random complete pending radio moving partial expired busy unavailable stale contradict missingModel missingSettings remove;do "$scratch/restore" "$case";done

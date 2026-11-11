#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)";trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch" <<'PY'
from pathlib import Path
import sys
repo,out=map(Path,sys.argv[1:]);text=(repo/'src/SSDP.cpp').read_text();parts=[]
for signature in ['SSDPClass::SSDPClass()', 'SSDPClass::~SSDPClass()', 'bool SSDPClass::begin()', 'void SSDPClass::end()', 'bool SSDPClass::_startsWith(', 'void SSDPClass::_parsePacket(', 'void SSDPClass::_addToSendQueue(', 'void SSDPClass::_sendQueuedResponses(', 'void SSDPClass::_processRequest(', 'void SSDPClass::_dispatchRequest(', 'void SSDPClass::_drainRequests(', 'void SSDPClass::_resetRequests(', 'uint32_t SSDPClass::_requestEpoch(', 'bool SSDPClass::_enqueueRequest(', 'bool SSDPClass::_takeRequest(', 'void SSDPClass::loop()']:
 if signature not in text:continue
 at=text.index(signature)
 if signature=='SSDPClass::SSDPClass()':
  parts.append(text[at:text.index('\n',at)]);continue
 brace=text.index('{',at);end=brace+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 parts.append(text[at:end])
(out/'production-ssdp.inc').write_text('\n'.join(parts))
PY
"${CXX:-c++}" -std=c++17 -O2 -pthread -include vector -I"$scratch" -I"$repo/tests/ssdp-ownership/stubs" -I"$repo/src" "$repo/tests/ssdp-ownership/test.cpp" -o "$scratch/test"
"$scratch/test"

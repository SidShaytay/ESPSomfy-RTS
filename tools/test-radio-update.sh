#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch/production-radio.inc" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);text=(root/'src/Transceiver.cpp').read_text();out=[]
for name in ('radioUpdateResourcesReady','somfy_rx_queue_t::init','Transceiver::begin','Transceiver::updateActivityLocked','Transceiver::updateActivity','Transceiver::pauseForInstall','Transceiver::resumeAfterInstall','Transceiver::installPauseHeld','Transceiver::enableReceive','Transceiver::disableReceive','Transceiver::clearReceived','Transceiver::handleReceive','Transceiver::receive'):
 at=text.find(name+'(')
 if at<0:continue
 start=text.rfind('\n',0,at)+1;brace=text.index('{',at);end=brace+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 out.append(text[start:end])
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
PY
# The unchanged decoder has an existing sign-compare warning; keep strict
# diagnostics on the extracted admission/ISR methods under review.
"${CXX:-g++}" -std=c++17 -O2 -I"$repo/src" -I"$repo/test/stubs" -c "$repo/src/PulseDecoder.cpp" -o "$scratch/pulse.o"
"${CXX:-g++}" -std=c++17 -O2 -pthread -Wall -Wextra -Werror -Wno-unused-function \
 -I"$scratch" -I"$repo/src" -I"$repo/test/stubs" \
 "$repo/tests/radio-update/radio-test.cpp" "$repo/src/RadioScheduler.cpp" "$scratch/pulse.o" \
 -o "$scratch/radio"
"$scratch/radio"

#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch/production-counter.inc" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);out=[]
for path,names in [('SomfyRemote.cpp',['SomfyRemote::setRemoteAddress','SomfyRemote::getRemoteAddress','SomfyRemote::p_lastRollingCode','SomfyLinkedRemote::SomfyLinkedRemote']),('SomfyGroup.cpp',['SomfyGroup::clear']),('SomfyShade.cpp',['SomfyShade::clear','SomfyShade::onChanged','SomfyShade::getFlags','SomfyShade::setFlags','SomfyShade::p_lastRollingCode','SomfyShade::markTxPending','SomfyShade::processTxEvent']),('ShadeController.cpp',['SomfyShadeController::getShadeById']),('Transceiver.cpp',['Transceiver::reserveCounterEdit','Transceiver::finishCounterEdit','Transceiver::reserveCounterFileRecovery','Transceiver::finishCounterFileRecovery','Transceiver::counterFileRecoveryAddress','Transceiver::counterQuarantine','Transceiver::counterBlocksIdentity','Transceiver::counterEditUncertain'])]:
    text=(root/'src'/path).read_text()
    for name in names:
        at=text.index(name+'(');start=text.rfind('\n',0,at)+1;brace=text.index('{',at);end=brace+1;depth=1
        while depth:
            depth+=(text[end]=='{')-(text[end]=='}');end+=1
        out.append(text[start:end])
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
PY
"${CXX:-c++}" -std=c++17 -O2 -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -I"$scratch" -I"$json" -I"$repo/test/stubs" -I"$repo/src" \
  "$repo/tests/explicit-counter/counter-test.cpp" "$repo/src/CounterEdit.cpp" "$repo/src/GitUpdate.cpp" \
  "$repo/src/RadioScheduler.cpp" "$repo/src/ShadeBehaviour.cpp" "$repo/src/ShadeSensors.cpp" "$repo/src/ShadeMotion.cpp" "$repo/src/TxBuffer.cpp" \
  -o "$scratch/counter"
"$scratch/counter"

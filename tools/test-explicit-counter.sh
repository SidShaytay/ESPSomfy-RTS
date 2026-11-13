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
for path,names in [('SomfyRemote.cpp',['SomfyRemote::setRemoteAddress','SomfyRemote::getRemoteAddress','SomfyRemote::p_lastRollingCode','SomfyRemote::transformCommand','SomfyLinkedRemote::SomfyLinkedRemote']),('SomfyGroup.cpp',['SomfyGroup::clear','SomfyGroup::sendCommand','SomfyGroup::sendCommandReceipt','SomfyGroup::hasShadeId','SomfyGroup::p_direction','SomfyGroup::updateFlags']),('SomfyShade.cpp',['SomfyShade::clear','SomfyShade::onChanged','SomfyShade::getFlags','SomfyShade::setFlags','SomfyShade::p_lastRollingCode','SomfyShade::markTxPending','SomfyShade::processTxEvent','SomfyShade::processInternalCommand','SomfyShade::checkMovement','SomfyShade::findRemote','SomfyShade::processFrame','SomfyShade::unlinkRemote']),('ShadeCommands.cpp',['SomfyShade::prepareCommand','SomfyShade::userOverride']),('SomfyGroupTarget.cpp',['SomfyGroup::updateTargetDirection']),('ShadeController.cpp',['SomfyShadeController::getShadeById','SomfyShadeController::processFrame','SomfyShadeController::getGroupById','SomfyShadeController::getRoomById','SomfyShadeController::cancelGroupStartsForShade','SomfyShadeController::cancelGroupReplansForShade']),('outputs/SomfyEvents.cpp',['SomfyGroup::emitState']),('LoopQueue.cpp',['LoopQueue::begin','LoopQueue::submit','LoopQueue::pending']),('Transceiver.cpp',['Transceiver::cancelQueuedForRemote','Transceiver::reserveCounterEdit','Transceiver::finishCounterEdit','Transceiver::reserveCounterFileRecovery','Transceiver::finishCounterFileRecovery','Transceiver::counterFileRecoveryAddress','Transceiver::counterQuarantine','Transceiver::counterBlocksIdentity','Transceiver::counterEditUncertain','Transceiver::blockedCounterAddress','Transceiver::submitTx','Transceiver::serviceRepeater','Transceiver::txSuspended'])]:
    text=(root/'src'/path).read_text()
    for name in names:
        search=0
        while True:
            at=text.find(name+'(',search)
            if at<0: break
            start=text.rfind('\n',0,at)+1;brace=text.index('{',at);end=brace+1;depth=1
            while depth:
                depth+=(text[end]=='{')-(text[end]=='}');end+=1
            out.append(text[start:end]);search=end
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
# Compile the actual dispatcher prefix plus link/unlink case unchanged. Other
# mutation cases are excluded, not replaced with a test implementation.
text=(root/'src/SomfyJson.cpp').read_text()
def body(name):
    at=text.index(name+'(');start=text.rfind('\n',0,at)+1;brace=text.index('{',at);end=brace+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
a=text.index('edit_result_t mutateModelEdit(');switch=text.index('  switch(edit.type) {',a)
case=text.index('    case model_edit_type_t::link_remote:',switch);brace=text.index('{',case);end=brace+1;depth=1
while depth:
    depth+=(text[end]=='{')-(text[end]=='}');end+=1
output=body('done')+'\n'+text[a:switch]+'  switch(edit.type) {\n'+text[case:end]+'\n default: return edit_result_t::error("Unrelated case excluded by harness");\n }\n}\n'
Path(sys.argv[2]).with_name('production-link-dispatch.inc').write_text(output)
text=(root/'src/ModelEditRuntime.cpp').read_text();out=[]
for name in ('markEdited','applyModelEdit'):
    out.append(body(name))
Path(sys.argv[2]).with_name('production-link-fanout.inc').write_text('\n'.join(out)+'\n')
PY
"${CXX:-c++}" -std=c++17 -O2 -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -I"$scratch" -I"$json" -I"$repo/test/stubs" -I"$repo/tests/ui-adapter" -I"$repo/src" \
  "$repo/tests/explicit-counter/counter-test.cpp" "$repo/src/CounterEdit.cpp" "$repo/src/GitUpdate.cpp" \
  "$repo/src/RadioScheduler.cpp" "$repo/src/ShadeBehaviour.cpp" "$repo/src/ShadeGroupMotion.cpp" "$repo/src/ShadeSensors.cpp" "$repo/src/ShadeMotion.cpp" "$repo/src/TxBuffer.cpp" "$repo/src/SomfyFrame.cpp" "$repo/src/CounterResults.cpp" "$repo/src/ModelEditor.cpp" "$repo/src/ModelPersistence.cpp" \
  -o "$scratch/counter"
cases=(primaryCompletion reviewedPrimary badLoad core completedGroup unknownGroup pending pendingRecovery commandGuard sink aliasRisk learnBound browserCommit globalIdle sharedLink)
if (($#));then cases=("$@");fi
for case in badLoad "${cases[@]}";do "$scratch/counter" "$case";done

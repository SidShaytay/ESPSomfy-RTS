#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
if [[ ! -f "$json/ArduinoJson.h" ]]; then json="$repo/../main/.pio/libdeps/native/ArduinoJson/src"; fi
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch/production.inc" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);out=[]
# Preserve real admission, self-echo and model fan-out. Only NVS and physical
# radio execution are fake. Extracted methods are a repository harness convention.
for path,names in [
 ('SomfyRemote.cpp',['SomfyRemote::setRemoteAddress','SomfyRemote::getRemoteAddress','SomfyRemote::transformCommand','SomfyRemote::sendCommand','SomfyRemote::sendCommandAfter','SomfyRemote::isLastCommand','SomfyRemote::p_lastRollingCode','SomfyLinkedRemote::SomfyLinkedRemote']),
 ('SomfyShade.cpp',['SomfyShade::onChanged','SomfyShade::p_lastRollingCode','SomfyShade::getFlags','SomfyShade::findRemote','SomfyShade::processFrame','SomfyShade::processInternalCommand','SomfyShade::checkMovement','SomfyShade::markTxPending','SomfyShade::processTxEvent','SomfyShade::setPosition','SomfyShade::onSaveWanted','SomfyShade::prepareCommand','SomfyShade::userOverride','SomfyShade::supportsGroupTarget','SomfyShade::moveToGroupTarget','SomfyShade::stopGroupTarget']),
 ('ShadeController.cpp',['SomfyShadeController::processFrame','SomfyShadeController::markTxPending','SomfyShadeController::processTxEvent','SomfyShadeController::cancelGroupStartsForShade','SomfyShadeController::cancelGroupReplansForShade'])]:
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
PY
extra_sources=()
if [[ -f "$repo/src/ShadeGroupMotion.cpp" ]]; then extra_sources+=("$repo/src/ShadeGroupMotion.cpp"); fi
"${CXX:-c++}" -std=c++17 -O0 -g -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections \
 -I"$scratch" -I"$json" -I"$repo/test/stubs" -I"$repo/src" \
 "$repo/tests/group-target/group-target-test.cpp" "$repo/src/ShadeBehaviour.cpp" "$repo/src/ShadeCalibratedMotion.cpp" "$repo/src/LiftCalibration.cpp" "$repo/src/ShadeMotion.cpp" \
 "$repo/src/ShadeSensors.cpp" "$repo/src/SomfyFrame.cpp" "$repo/src/RadioScheduler.cpp" "$repo/src/TxBuffer.cpp" "${extra_sources[@]}" \
 -o "$scratch/group-target"
if (( $# )); then cases=("$@"); else
 cases=(calibration-exclusive-gate calibrated-raw-group-reversal calibrated-deadlines calibrated-endpoint-one-settling calibrated-endpoint-both-settling calibrated-refused-start calibrated-refused-retarget calibrated-dropped-start calibrated-aborted-start calibrated-aborted-stop shared unequal repeated pending-replace queued-stop onair-stop member-onair-stop member-queued idle-stop mixed refused refused-member-override refused-group-override flag-preserves-reversal deadband flipped dropped-start aborted-start sparse-members favorite-program-queued favorite-program-aborted favorite-program-finished stop-program-queued favorite integrated-group short-step step stale-stop reversal)
fi
for case in "${cases[@]}"; do "$scratch/group-target" "$case"; done

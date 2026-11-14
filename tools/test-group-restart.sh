#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch/production-group-domain.inc" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);out=[]
for path,names in [('SomfyRemote.cpp',['SomfyRemote::setRemoteAddress','SomfyRemote::getRemoteAddress','SomfyRemote::p_lastRollingCode','SomfyRemote::transformCommand','SomfyLinkedRemote::SomfyLinkedRemote']),('SomfyGroup.cpp',['SomfyGroup::sendCommand','SomfyGroup::sendCommandReceipt','SomfyGroup::p_direction','SomfyGroup::updateFlags']),('SomfyShade.cpp',['SomfyShade::onChanged','SomfyShade::getFlags','SomfyShade::p_lastRollingCode','SomfyShade::processInternalCommand','SomfyShade::checkMovement','SomfyShade::markTxPending','SomfyShade::processTxEvent']),('SomfyGroupTarget.cpp',['SomfyGroup::updateTargetDirection']),('ShadeCommands.cpp',['SomfyShade::supportsGroupTarget']),('outputs/SomfyEvents.cpp',['SomfyGroup::emitState'])]:
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
"${CXX:-c++}" -std=c++17 -O2 -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections \
 -I"$scratch" -I"$json" -I"$repo/test/stubs" -I"$repo/src" \
 "$repo/tests/legacy-restart/group-domain-test.cpp" "$repo/src/ShadeBehaviour.cpp" "$repo/src/ShadeCalibratedMotion.cpp" "$repo/src/LiftCalibration.cpp" "$repo/src/ShadeGroupMotion.cpp" "$repo/src/ShadeMotion.cpp" "$repo/src/ShadeSensors.cpp" "$repo/src/SomfyFrame.cpp" "$repo/src/GitUpdate.cpp" "$repo/src/SettingsDurability.cpp" "$repo/src/RestartCoordinator.cpp" "$repo/src/RestartPolicy.cpp" "$repo/src/SettingsValues.cpp" \
 -o "$scratch/group"
for case in favorite up down moving empty dangling unknownUp unknownDown unknownMy unknownToggle unknownStepUp unknownStepDown unknownFavorite;do "$scratch/group" "$case";done

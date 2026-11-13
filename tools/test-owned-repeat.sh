#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);out=Path(sys.argv[2])
# Reuse the established hardware-only model seam, adding the group container
# required to compile actual group receipt bookkeeping (not a fake dispatcher).
s=(root/'tests/gpio-command/Model.h').read_text()


s=s.replace('struct SomfyShadeController {','''struct SomfyGroup:SomfyRemote {
 uint8_t linkedShades[32]={};int8_t direction=0;unsigned events=0;
 uint8_t getGroupId(){return 1;}int8_t p_direction(int8_t dir){auto old=direction;direction=dir;return old;}
 void updateFlags(){}void emitState(){++events;}
 send_result_t sendCommandReceipt(somfy_commands,uint8_t,uint8_t);
};
struct SomfyShadeController {''')
s=s.replace('unsigned frames=0,processed=0;','SomfyGroup group;unsigned frames=0,processed=0;\n SomfyShade *getShadeById(uint8_t id){return id==1?&shades[0]:nullptr;}\n SomfyGroup *getGroupById(uint8_t id){return id==1?&group:nullptr;}')
s=s.replace('bool submitTx(somfy_tx_job_t &){return true;}','uint32_t next=200;bool reject=false;bool submitTx(somfy_tx_job_t &j){if(reject)return false;j.ticket=++next;return true;}')
s=s.replace('++frames;return 1;','++frames;return transceiver.reject?0:++transceiver.next;')
s=s.replace('bool isIdle(){','void processInternalCommand(somfy_commands c,uint8_t r,uint8_t step){behaviour.onGroupCommand(c,r,0,step);}\n void emitCommand(somfy_commands,const char *,uint32_t){}\n bool isIdle(){')
(out/'Model.h').write_text(s)
def body(text,name):
 a=text.index(name+'(');start=text.rfind('\n',0,a)+1;b=text.index('{',a);end=b+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
rx=(root/'src/SomfyShade.cpp').read_text()
(out/'production-rx.inc').write_text('\n'.join(body(rx,n) for n in ('SomfyShade::findRemote','SomfyShade::processFrame')))
(out/'production-owned.inc').write_text(body((root/'src/SomfyGroup.cpp').read_text(),'SomfyGroup::sendCommandReceipt')+'\n'+body((root/'src/SomfyShadeOps.cpp').read_text(),'SomfyShadeOps::sendOwnedRepeat'))
# Reuse hardware/NVS notification substitutes and leave old cases unused.
s=(root/'tests/gpio-command/test.cpp').read_text().replace('#include "Model.h"','').replace('int main(){','void legacyMain(){')
(out/'hardware.cpp').write_text(s)
PY
"${CXX:-c++}" -std=c++17 -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections \
 -include "$scratch/Model.h" -I"$scratch" -I"$repo/tests/gpio-command" -I"$repo/test/stubs" -I"$repo/src" \
 "$scratch/hardware.cpp" "$repo/tests/owned-repeat/test.cpp" \
 "$repo/src/SomfyRemote.cpp" "$repo/src/ShadeCommands.cpp" "$repo/src/ShadeGpioCommands.cpp" "$repo/src/SomfyFrame.cpp" "$repo/src/ShadeGpio.cpp" "$repo/src/LivePinClaims.cpp" "$repo/src/GpioPinClaims.cpp" "$repo/src/ShadeTransport.cpp" "$repo/src/ShadeBehaviour.cpp" "$repo/src/ShadeGroupMotion.cpp" "$repo/src/ShadeMotion.cpp" "$repo/src/ShadeSensors.cpp" "$repo/src/ShadeRequests.cpp" \
 -o "$scratch/owned-repeat"
"$scratch/owned-repeat"

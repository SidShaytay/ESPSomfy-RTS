#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)";trap 'rm -rf -- "$scratch"' EXIT
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
python3 - "$repo" "$scratch" <<'PY'
from pathlib import Path
import sys
root,out=map(Path,sys.argv[1:])
fixture=(root/'tests/radio-update/radio-test.cpp').read_text().split('int main()')[0]
fixture=fixture.replace('#include "RadioUpdateActivity.h"','#include "RadioUpdateActivity.h"\n#include "GitUpdate.h"\n#include "SettingsDurability.h"\n#include "RadioSettingsAdmission.h"\n#include <atomic>\n#include <string>')
old='  struct {SettingsLoadStatus loadStatus{SettingsLoadState::loaded};bool enabled=true,noiseDetection=false,radioInit=true;int RXPin=8;void apply(){assert(!rxHeld);}}config;'
assert old in fixture
fixture=fixture.replace('struct Transceiver {','struct transceiver_config_t {SettingsLoadStatus loadStatus{SettingsLoadState::loaded};bool enabled=true,noiseDetection=false,radioInit=true;int RXPin=8,TXPin=3;void applyBoardPins(){}settings_save_result_t save();settings_save_result_t apply();};\nstruct Transceiver {')
fixture=fixture.replace(old,'  transceiver_config_t config;\n  settings_save_result_t setConfig(const transceiver_config_t &);\n  settings_save_result_t setConfigWhilePaused(const transceiver_config_t &,const RadioInstallToken &,const GitUpdateOwnerToken &);')
fixture=fixture.replace('explicit RadioLock(unsigned=0)','explicit RadioLock(unsigned=0,bool=false)')
fixture=fixture.replace('struct Shade{uint8_t id=255;','struct Shade{bool idle=true;bool idleForAdministration()const{return idle;}uint8_t id=255;')
fixture=fixture.replace(' bool isDirty=false;', ' struct Group{uint8_t id=255;int direction=0;somfy_frame_t lastFrame;uint8_t linkedShades[32]={};uint8_t getGroupId()const{return id;}}groups[16];\n Shade *getShadeById(uint8_t id){for(auto &s:shades)if(s.id==id)return &s;return nullptr;}\n bool isDirty=false;')
fixture=fixture.replace('#include "production-radio.inc"','''#include "production-radio.inc"
struct LoopQueue {std::atomic<bool> restartSealed{false};std::atomic<unsigned>producers{0};unsigned work=0;unsigned pending()const{return work;}bool sealForRestart();void reopenAfterRefusedRestart();}loopQueue;
#include "production-queue-seal.inc"
static constexpr unsigned portMAX_DELAY=0;
''')
(out/'test.cpp').write_text(fixture+'\nbool restartPending();\n#include \"RadioSettingsAdmission.cpp\"\n'+(root/'tests/radio-settings/admission-test.cpp').read_text())
def body(text,name):
 at=text.index(name+'(');start=text.rfind('\n',0,at)+1;brace=text.index('{',at);end=brace+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
text=(root/'src/Transceiver.cpp').read_text()
names=('radioUpdateResourcesReady','somfy_rx_queue_t::init','Transceiver::begin','Transceiver::updateActivityLocked','Transceiver::updateActivity','Transceiver::pauseForAdministration','Transceiver::pauseForInstall','Transceiver::pauseForRestore','Transceiver::serviceUntrustedReceive','Transceiver::resumeAfterInstall','Transceiver::installPauseHeld','Transceiver::enableReceive','Transceiver::disableReceive','Transceiver::clearReceived','Transceiver::handleReceive','Transceiver::receive')
source='\n'.join(body(text,n)for n in names)
source+='\n'+body((root/'src/ShadeController.cpp').read_text(),'SomfyShadeController::loop')
header=(root/'src/Transceiver.h').read_text();at=header.index('bool available() const');brace=header.index('{',at);end=header.index('}',brace)+1
source+='\nbool Transceiver::available() const '+header[brace:end]
(out/'production-radio.inc').write_text(source)
q=(root/'src/LoopQueue.cpp').read_text();(out/'production-queue-seal.inc').write_text(body(q,'LoopQueue::sealForRestart')+'\n'+body(q,'LoopQueue::reopenAfterRefusedRestart'))
settings=body(text,'Transceiver::setConfig')
if 'Transceiver::setConfigWhilePaused(' in text:settings+='\n'+body(text,'Transceiver::setConfigWhilePaused')
(out/'production-radio-settings.inc').write_text(settings)
PY
"${CXX:-c++}" -std=c++17 -O2 -pthread -I"$repo/src" -I"$json" -c "$repo/src/GitUpdate.cpp" -o "$scratch/owner.o"
"${CXX:-c++}" -std=c++17 -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections -pthread -DRADIO_SETTINGS_HOST_TEST -I"$scratch" -I"$repo/src" -I"$repo/test/stubs" -I"$json" \
 "$scratch/test.cpp" "$repo/src/RadioScheduler.cpp" "$repo/src/PulseDecoder.cpp" "$repo/src/SettingsDurability.cpp" "$repo/src/SomfyFrame.cpp" "$scratch/owner.o" -o "$scratch/test"
for mode in owner movement held group groupCompleted pending producer radio scan restart untrusted lateMovement marker persist busyCleanup success;do "$scratch/test" "$mode";done

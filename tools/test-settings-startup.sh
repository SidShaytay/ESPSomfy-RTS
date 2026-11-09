#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)";trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch" <<'PY'
from pathlib import Path
import sys
root,out=map(Path,sys.argv[1:]);parts=[]
def body(text,name):
 at=text.index(name+'(');start=text.rfind('\n',0,at)+1;brace=text.index('{',at);end=brace+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
cfg=(root/'src/ConfigSettings.cpp').read_text();whole=body(cfg,'ConfigSettings::begin');at=whole.index('  this->getAppVersion();');parts.append('bool ConfigSettings::begin(){\n'+whole[at:])
parts.append(body(cfg,'ConfigSettings::applyStartup'))
radio=(root/'src/Transceiver.cpp').read_text();parts.extend(body(radio,name) for name in ['Transceiver::loadReadOnly','transceiver_config_t::apply'])
setup=(root/'src/SomfyController.cpp').read_text();start=setup.index('  settings.begin();');end=setup.index('  if(WiFi.status()',start)
parts.append('void startupReads(){\n'+setup[start:end]+'\n}')
network=(root/'src/ESPNetwork.cpp').read_text();parts.extend(body(network,name) for name in ['ESPNetwork::setup','ESPNetwork::openSoftAP'])
start=network.index('  clientScanLoop(*this);');end=network.index('  if(this->connecting())',start)
parts.append('void ESPNetwork::loopAdmission(){\n'+network[start:end]+'\n}')
(out/'production-settings-startup.inc').write_text('\n'.join(parts))
PY
for variant in generic bundled;do
 flags=();[[ "$variant" != bundled ]] || flags=(-DESPSOMFY_LILYGO_T_EMBED_CC1101 -DCONFIG_IDF_TARGET_ESP32S3)
 "${CXX:-c++}" -std=c++17 -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections "${flags[@]}" \
  -I"$scratch" -I"$json" -I"$repo/test/stubs" -I"$repo/src" \
  "$repo/tests/settings-startup/startup-test.cpp" "$repo/src/SettingsNamespaceLoad.cpp" "$repo/src/SettingsRadioLoad.cpp" \
  "$repo/src/SettingsTypedValidation.cpp" "$repo/src/SettingsLoad.cpp" "$repo/src/SecurityLoad.cpp" "$repo/src/SecuritySettingsLoad.cpp" \
  "$repo/src/SettingsValues.cpp" "$repo/src/SettingsPersistence.cpp" "$repo/src/RadioInitMarker.cpp" "$repo/src/SettingsDurability.cpp" "$repo/src/ProtoSettings.cpp" -o "$scratch/startup"
 for case in networkFailed networkKnown startup startupFailed unknown markerFalse readFailed openFailed firstWriteFailed lastWriteFailed detectedFailed success;do "$scratch/startup" "$case";done
done

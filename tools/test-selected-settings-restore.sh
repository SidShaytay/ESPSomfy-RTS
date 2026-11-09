#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)";trap 'rm -rf -- "$scratch"' EXIT
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
unity="${UNITY_INCLUDE:-$repo/.pio/libdeps/native/Unity/src}"
python3 - "$repo/src/ConfigSettings.cpp" "$scratch/ethernet-claims.inc" <<'PY'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text();at=s.index('bool EthernetSettings::usesPin(');brace=s.index('{',at);end=brace+1;depth=1
while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
Path(sys.argv[2]).write_text(s[at:end])
PY
for variant in wifi ethernet;do
 flags=();[[ "$variant" != ethernet ]] || flags=(-DCONFIG_ETH_USE_ESP32_EMAC=1)
 "${CXX:-c++}" -std=c++17 -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections "${flags[@]}" -I"$scratch" -I"$repo/test/stubs" -I"$repo/src" -I"$json" -I"$unity" \
 "$repo/tests/selected-settings/test.cpp" "$repo/src/SelectedSettingsRestore.cpp" "$repo/src/SettingsTypedValidation.cpp" \
 "$repo/src/SettingsPersistence.cpp" "$repo/src/SettingsValues.cpp" "$repo/src/SettingsLoad.cpp" "$repo/src/SettingsDurability.cpp" \
 "$repo/src/ConfigRecords.cpp" "$repo/src/Utils.cpp" "$repo/src/ModelEditor.cpp" \
 "$repo/src/ProtoSettings.cpp" "$repo/src/ProtoParse.cpp" "$repo/src/ProtoSchema.cpp" "$unity/unity.c" -o "$scratch/test"
 "$scratch/test"
done

#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)";trap 'rm -rf -- "$scratch"' EXIT
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
python3 - "$repo/src/ESPNetwork.cpp" "$scratch/production-ethernet.inc" <<'PYCODE'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text();at=s.index('bool ESPNetwork::connectWired(');brace=s.index('{',at);end=brace+1;depth=1
while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
Path(sys.argv[2]).write_text(s[at:end])
PYCODE
for variant in wifi ethernet;do
 flags=();[[ "$variant" != ethernet ]] || flags=(-DCONFIG_ETH_USE_ESP32_EMAC=1)
 "${CXX:-c++}" -std=c++17 -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections -pthread "${flags[@]}" -I"$scratch" -I"$repo/test/stubs" -I"$repo/src" -I"$json" \
 "$repo/tests/pin-ownership/test.cpp" "$repo/src/PinOwnershipContext.cpp" "$repo/src/LivePinClaims.cpp" "$repo/src/GpioPinClaims.cpp" "$repo/src/SettingsTypedValidation.cpp" "$repo/src/SettingsValues.cpp" "$repo/src/ProtoSettings.cpp" "$repo/src/ProtoParse.cpp" "$repo/src/ProtoSchema.cpp" -o "$scratch/test"
 "$scratch/test"
done

#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)";trap 'rm -rf -- "$scratch"' EXIT
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
unity="${UNITY_INCLUDE:-$repo/.pio/libdeps/native/Unity/src}"
# Run the same actual Form regression under both compile-time board policies.
python3 - "$repo" "$scratch/test.cpp" <<'PY'
from pathlib import Path
import sys
source=(Path(sys.argv[1])/'test/test_localsettingsform/test_localsettingsform.cpp').read_text()
Path(sys.argv[2]).write_text(source[:source.rindex('int main(')]+'''int main(){UNITY_BEGIN();RUN_TEST(unsupported_frequency_requires_explicit_valid_replacement_without_losing_other_drafts);return UNITY_END();}''')
PY
for variant in generic bundled;do
 flags=();[[ "$variant" != bundled ]] || flags=(-DESPSOMFY_LILYGO_T_EMBED_CC1101 -DCONFIG_IDF_TARGET_ESP32S3)
 "${CXX:-c++}" -std=c++17 -O2 "${flags[@]}" -I"$repo/test/stubs" -I"$repo/src" -I"$json" -I"$unity" \
  "$scratch/test.cpp" "$repo/src/LocalSettingsForm.cpp" "$repo/src/ProtoSettings.cpp" "$repo/src/ProtoParse.cpp" "$repo/src/ProtoSchema.cpp" "$repo/src/JsonFormatter.cpp" "$unity/unity.c" -o "$scratch/test"
 "$scratch/test"
done

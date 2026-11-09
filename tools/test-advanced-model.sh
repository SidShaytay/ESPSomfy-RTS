#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
# Compile the actual model-edit group branch and Group list methods unchanged.
# Only hardware, notification and file endpoints are replaced by bounded fakes.
python3 - "$repo" "$scratch" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);out=Path(sys.argv[2]);text=(root/'src/SomfyJson.cpp').read_text()
a=text.index('    case model_edit_type_t::link_to_group:');b=text.index('    case model_edit_type_t::link_repeater:',a)
(out/'production-group-edit.inc').write_text(text[a:b])
text=(root/'src/SomfyGroup.cpp').read_text();methods=[]
for name in ['SomfyGroup::linkShade','SomfyGroup::unlinkShade']:
 a=text.index(name+'(');a=text.rfind('\n',0,a)+1;b=text.index('}',a)+1;methods.append(text[a:b])
(out/'production-group-list.inc').write_text('\n'.join(methods))
PY
"${CXX:-c++}" -std=c++17 -O2 -I"$scratch" -I"$json" -I"$repo/test/stubs" -I"$repo/src" \
 "$repo/tests/advanced-model/edit-test.cpp" "$repo/src/ModelEditor.cpp" "$repo/src/ModelPersistence.cpp" -o "$scratch/edit-test"
"$scratch/edit-test"

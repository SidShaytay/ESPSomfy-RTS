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
def body(text,name):
 a=text.index(name+'(');start=text.rfind('\n',0,a)+1;brace=text.index('{',a);end=brace+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
a=text.index('    case model_edit_type_t::link_to_group:');b=text.index('    case model_edit_type_t::link_repeater:',a)
prefix=text[text.index('edit_result_t mutateModelEdit('):text.index('  switch(edit.type) {',text.index('edit_result_t mutateModelEdit('))]
helpers=body(text,'done')+'\n'
if 'applyGroupMembershipEdit(' in text:helpers+=body(text,'applyGroupMembershipEdit')+'\n'
(out/'production-group-edit.inc').write_text(helpers+prefix+'  switch(edit.type) {\n'+text[a:b]+'default:return edit_result_t::error("Unrelated mutation excluded");\n}\n}\n')
runtime=(root/'src/ModelEditRuntime.cpp').read_text()
(out/'production-group-fanout.inc').write_text(body(runtime,'markEdited')+'\n'+body(runtime,'applyModelEdit'))
text=(root/'src/SomfyGroup.cpp').read_text();methods=[]
for name in ['SomfyGroup::linkShade','SomfyGroup::unlinkShade']:
 a=text.index(name+'(');a=text.rfind('\n',0,a)+1;b=text.index('}',a)+1;methods.append(text[a:b])
(out/'production-group-list.inc').write_text('\n'.join(methods))
PY
"${CXX:-c++}" -std=c++17 -O2 -I"$scratch" -I"$json" -I"$repo/test/stubs" -I"$repo/src" \
 "$repo/tests/advanced-model/edit-test.cpp" "$repo/src/ModelEditor.cpp" "$repo/src/ModelPersistence.cpp" "$repo/src/EntityFields.cpp" "$repo/src/ProtoEdits.cpp" "$repo/src/ProtoSchema.cpp" "$repo/src/ProtoParse.cpp" "$repo/src/JsonFormatter.cpp" -o "$scratch/edit-test"
"$scratch/edit-test"

#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -O2 -DESPSOMFY_LOCAL_UI -DESPSOMFY_LOCAL_COMMANDS \
  -I"$repo/tests/ui-adapter" -I"$repo/test/stubs" -I"$repo/src" \
  "$repo/tests/ui-adapter/adapter-test.cpp" "$repo/src/LocalUi.cpp" "$repo/src/LocalQuickAccess.cpp" "$repo/src/LocalControlPolicy.cpp" "$repo/src/ShadeRequests.cpp" "$repo/src/ShadeMotion.cpp" "$repo/src/SomfyFrame.cpp" "$repo/src/CounterResults.cpp" "$repo/src/ModelEditor.cpp" "$repo/src/ModelPersistence.cpp" -o "$scratch/adapter"
"$scratch/adapter"
"${CXX:-c++}" -std=c++17 -O2 -DESPSOMFY_LOCAL_UI \
  -I"$repo/tests/ui-adapter" -I"$repo/test/stubs" -I"$repo/src" \
  "$repo/tests/ui-adapter/authorization-test.cpp" -o "$scratch/authorization"
"$scratch/authorization"

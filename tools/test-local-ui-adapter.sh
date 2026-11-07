#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -O2 -DESPSOMFY_LOCAL_UI -DESPSOMFY_LOCAL_COMMANDS \
  -I"$repo/tests/ui-adapter" -I"$repo/test/stubs" -I"$repo/src" \
  "$repo/tests/ui-adapter/adapter-test.cpp" -o "$scratch/adapter"
"$scratch/adapter"

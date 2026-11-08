#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -O2 -I"$repo/tests/application-upload" -I"$repo/test/stubs" -I"$repo/src" \
  "$repo/tests/application-upload/application-upload-test.cpp" -o "$scratch/application-upload"
"$scratch/application-upload"

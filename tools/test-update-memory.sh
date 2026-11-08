#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -DESPSOMFY_LILYGO_T_EMBED_CC1101 \
  -I"$repo/tests/update-memory" -I"$repo/src" \
  "$repo/tests/update-memory/memory-test.cpp" "$repo/src/GitUpdateMemory.cpp" -o "$scratch/test"
"$scratch/test"
printf 'Bounded TLS allocator checks passed\n'

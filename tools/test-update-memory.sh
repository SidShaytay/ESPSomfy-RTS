#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -DESPSOMFY_GITHUB_UPDATES \
  -I"$repo/tests/update-memory" -I"$repo/src" \
  "$repo/tests/update-memory/memory-test.cpp" "$repo/src/GitUpdateMemory.cpp" \
  -o "$scratch/memory-test"
"$scratch/memory-test"
echo 'TLS allocator policy checks passed'
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I"$repo/src" \
  "$repo/tests/update-memory/health-test.cpp" -o "$scratch/health-test"
"$scratch/health-test"
echo 'Continuous boot health checks passed'

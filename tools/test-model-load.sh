#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I"$repo/src" "$repo/tests/model-load/snapshot-test.cpp" "$repo/src/ModelLoadSnapshot.cpp" -o "$scratch/test"
"$scratch/test"

python3 "$repo/tests/model-load/run.py"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -DESPSOMFY_LILYGO_T_EMBED_CC1101=1 \
 -I"$repo/tests/model-load" -I"$repo/src" "$repo/tests/model-load/caps-test.cpp" "$repo/src/ModelLoadSnapshot.cpp" -o "$scratch/caps"
"$scratch/caps"

#!/usr/bin/env bash
# Host-only read/hash/caching checks. No device access.
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf -- "$build"' EXIT
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror \
  -DESPSOMFY_LILYGO_T_EMBED_CC1101 \
  -I"$repo/tests/update-bootloader/stubs" -I"$repo/src" \
  "$repo/tests/update-bootloader/bootloader-test.cpp" \
  "$repo/src/GitUpdateBootloader.cpp" -o "$build/test-bootloader"
for mode in valid tampered read-error start-error hash-error finish-error encrypted; do
  "$build/test-bootloader" "$mode"
done
printf 'Bootloader attestation host checks passed\n'
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -I"$repo/src" \
  "$repo/tests/update-bootloader/unsupported-test.cpp" \
  "$repo/src/GitUpdateBootloader.cpp" "$repo/src/GitUpdateAssets.cpp" -o "$build/unsupported"
"$build/unsupported"

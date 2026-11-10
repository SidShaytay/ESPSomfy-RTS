#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
fs="${ARDUINO_FS_DIR:-$HOME/.platformio/packages/framework-arduinoespressif32/libraries/FS/src}"
[[ -f "$fs/FS.cpp" && -f "$fs/FSImpl.h" ]] || { echo 'Set ARDUINO_FS_DIR to the pinned Arduino FS sources.' >&2; exit 1; }
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -O1 -g -Wall -Wextra -Werror -pthread -I"$repo/tests/gated-filesystem" -I"$repo/src" -I"$fs" \
 "$repo/tests/gated-filesystem/gate-test.cpp" "$repo/src/GatedReadOnlyFilesystem.cpp" "$fs/FS.cpp" -o "$scratch/gate"
"$scratch/gate"

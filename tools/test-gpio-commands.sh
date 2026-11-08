#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -include "$repo/tests/gpio-command/Model.h" \
 -I"$repo/tests/gpio-command" -I"$repo/test/stubs" -I"$repo/src" \
 "$repo/tests/gpio-command/test.cpp" "$repo/src/SomfyRemote.cpp" "$repo/src/ShadeCommands.cpp" "$repo/src/ShadeGpioCommands.cpp" "$repo/src/SomfyFrame.cpp" "$repo/src/ShadeGpio.cpp" "$repo/src/ShadeTransport.cpp" "$repo/src/ShadeBehaviour.cpp" "$repo/src/ShadeMotion.cpp" "$repo/src/ShadeSensors.cpp" \
 -o "$scratch/commands"
"$scratch/commands"

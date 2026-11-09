#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch/production-rx.inc" <<'PYCODE'
from pathlib import Path
import sys
text=(Path(sys.argv[1])/'src/SomfyShade.cpp').read_text();out=[]
for name in ('SomfyShade::findRemote','SomfyShade::processFrame'):
    at=text.index(name+'(');start=text.rfind('\n',0,at)+1;brace=text.index('{',at);end=brace+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    out.append(text[start:end])
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
PYCODE
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -include "$repo/tests/gpio-command/Model.h" \
 -I"$scratch" -I"$repo/tests/gpio-command" -I"$repo/test/stubs" -I"$repo/src" \
 "$repo/tests/gpio-command/test.cpp" "$repo/src/SomfyRemote.cpp" "$repo/src/ShadeCommands.cpp" "$repo/src/ShadeGpioCommands.cpp" "$repo/src/SomfyFrame.cpp" "$repo/src/ShadeGpio.cpp" "$repo/src/LivePinClaims.cpp" "$repo/src/GpioPinClaims.cpp" "$repo/src/ShadeTransport.cpp" "$repo/src/ShadeBehaviour.cpp" "$repo/src/ShadeMotion.cpp" "$repo/src/ShadeSensors.cpp" \
 -o "$scratch/commands"
"$scratch/commands"

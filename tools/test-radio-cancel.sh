#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch/production-radio-cancel.inc" <<'PY'
from pathlib import Path
import sys
text=(Path(sys.argv[1])/'src/Transceiver.cpp').read_text()
at=text.index('Transceiver::cancelTx(')
start=text.rfind('\n',0,at)+1
brace=text.index('{',at)
end=brace+1
depth=1
while depth:
    depth+=(text[end]=='{')-(text[end]=='}')
    end+=1
Path(sys.argv[2]).write_text(text[start:end]+'\n')
PY
"${CXX:-c++}" -std=c++17 -O2 -I"$scratch" -I"$repo/test/stubs" -I"$repo/src" \
  "$repo/tests/radio-cancel/radio-cancel-test.cpp" "$repo/src/RadioScheduler.cpp" \
  "$repo/src/TxBuffer.cpp" "$repo/src/SomfyFrame.cpp" -o "$scratch/radio-cancel"
"$scratch/radio-cancel"

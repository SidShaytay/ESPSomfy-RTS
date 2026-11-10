#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo/src/Transceiver.cpp" "$scratch/scan.inc" <<'PY'
from pathlib import Path
import sys
text=Path(sys.argv[1]).read_text();out=[]
for name in ['Transceiver::beginFrequencyScan','Transceiver::endFrequencyScan','Transceiver::processFrequencyScan']:
 at=text.index(name+'(');start=text.rfind('\n',0,at)+1;brace=text.index('{',at);end=brace+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 out.append(text[start:end])
loop=text.index('void Transceiver::loop()');a=text.index('  if(rxmode == 3 &&',loop);b=text.index('  if(rxmode == 3) {',a)
out.append('void Transceiver::serviceScanStop(){\n'+text[a:b]+'\n}')
Path(sys.argv[2]).write_text('\n'.join(out))
PY
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I"$scratch" -I"$repo/src" -I"$repo/test/stubs" "$repo/tests/frequency-restore/test.cpp" -o "$scratch/test"
"$scratch/test"

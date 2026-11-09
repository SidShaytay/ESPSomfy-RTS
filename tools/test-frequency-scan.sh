#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo/src/Transceiver.cpp" "$scratch/production-scan.inc" <<'PY'
from pathlib import Path
import sys
source=Path(sys.argv[1]).read_text()
out=[]
for name in ('Transceiver::beginFrequencyScan','Transceiver::endFrequencyScan','Transceiver::processFrequencyScan'):
 at=source.index(name+'(');start=source.rfind('\n',0,at)+1;brace=source.index('{',at);end=brace+1;depth=1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 out.append(source[start:end])
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
PY
"${CXX:-c++}" -std=c++17 -O2 -I"$scratch" -I"$repo/src" -I"$repo/test/stubs" \
 "$repo/tests/frequency-scan/scan-test.cpp" "$repo/src/RadioScheduler.cpp" -o "$scratch/scan"
"$scratch/scan"

#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)";trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch" <<'PY'
from pathlib import Path
import sys
repo,out=map(Path,sys.argv[1:]);text=(repo/'src/ESPNetwork.cpp').read_text();parts=[]
for name in ['setConnected','networkEvent','loop','discoveryLoop','connected','connecting','clearConnecting']:
 needle=('bool' if name in ['connected','connecting'] else 'void')+' ESPNetwork::'+name+'('
 if needle not in text:continue
 start=text.index(needle);brace=text.index('{',start);end=brace+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 parts.append(text[start:end])
(out/'production-network.inc').write_text('\n'.join(parts))
PY
for ethernet in 0 1;do
 "${CXX:-c++}" -std=c++17 -O2 -pthread -DHAS_ETHERNET="$ethernet" -I"$scratch" \
  "$repo/tests/network-discovery/test.cpp" -o "$scratch/test"
 "$scratch/test"
done

#!/usr/bin/env bash
# Compile the unchanged TX-event consumer prefix with instrumented boundaries.
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
python3 - "${TRANSCEIVER_SOURCE:-$repo/src/Transceiver.cpp}" "$scratch/production-event-loop.inc" <<'PY'
from pathlib import Path
import sys
source=Path(sys.argv[1]).read_text()
start=source.index('void Transceiver::loop() {')
end=source.index('  if(scanEmitPending.exchange(false))',start)
Path(sys.argv[2]).write_text(source[start:end]+'}\n')
PY
for ui in 0 1; do
  flags=()
  if [[ "$ui" == 1 ]]; then flags+=(-DESPSOMFY_LOCAL_UI); fi
  "${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror "${flags[@]}" \
    -I"$scratch" -I"$repo/test/stubs" -I"$repo/src" \
    "$repo/tests/local-event-forward/test.cpp" -o "$scratch/forward-$ui"
  "$scratch/forward-$ui"
done
printf 'TX outcomes: one consumer, model-before-LCD ordering, unlocked callbacks and generic isolation passed\n'

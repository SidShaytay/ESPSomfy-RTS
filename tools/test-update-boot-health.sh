#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
python3 - "$repo" "$scratch" <<'PY'
from pathlib import Path
import sys
root,out=map(Path,sys.argv[1:])
text=(root/'src/SomfyController.cpp').read_text()
start=text.index('  bool updateNetworkReady=');end=text.index('  esp_task_wdt_reset();',start)
utils=(root/'src/Utils.cpp').read_text();u=utils.index('bool otaProven(');v=utils.index('}',u)+1
body=utils[u:v]+'\nvoid health(){\n'+text[start:end]+'\n}\n'
p=text.index('void setup() {');q=text.index('  heap_caps_register_failed_alloc_callback',p)
body+=text[p:q].replace('void setup()', 'void startupPrefix()')+'\n}\n'
p=text.index('#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101)\n  if(!gitUpdateControllerPaused())')
q=text.index('  if(net.connected()',p)
body+='void modelPass(){\n'+text[p:q]+'\n}\n'
p=text.index('  if(net.connected()',q);q=text.index('  bool updateNetworkReady=',p)
body+='void presentationPass(){\n'+text[p:q]+'\n}\n'
(out/'production-boot-health.inc').write_text(body)
PY
"${CXX:-c++}" -std=c++17 -O2 -I"$scratch" -I"$repo/src" -DESPSOMFY_LILYGO_T_EMBED_CC1101 -DESPSOMFY_BUNDLED_WEB \
  "$repo/tests/update-boot-health/test.cpp" -o "$scratch/health"
for case in valid longGap network routes filesystem allocator graphics bootloader assets memory slow paused busy counter radio model settings durability dirty retry failedUntilDeadline deadline rollbackClock startup allocatorFailureStartup loop stopAfterMovement;do "$scratch/health" "$case";done
printf 'Actual canonical boot health: continuous interval, SDK retry, deadline intent, startup and paused loop passed\n'

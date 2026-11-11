#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)";trap 'rm -rf -- "$scratch"' EXIT
mkdir -p "$scratch/cache/pio-python/bin" "$scratch/out/static/obsolete/objects"
printf 'src/Removed.cpp:1:1:void obsolete()\t9999\tstatic\n' > "$scratch/out/static/obsolete/objects/old.su"
cat > "$scratch/cache/pio-python/bin/pio" <<'PY'
#!/usr/bin/env python3
import os
from pathlib import Path
root=Path(os.environ['PLATFORMIO_BUILD_DIR'])/'lilygo-t-embed-cc1101'
root.mkdir(parents=True)
(root/'firmware.elf').write_bytes(b'test-fixture-not-firmware')
(root/'firmware.bin').write_bytes(b'STACK_AUDIT schema=1 role= AUDIT_ACK schema=1' if os.environ.get('FAKE_AUDIT_CONTROL') else b'test-fixture-with-no-record')
(root/'test.cpp.su').write_text('src/Test.cpp:1:1:void current()\t64\tstatic\n')
PY
cat > "$scratch/compiler-g++" <<'SH'
#!/usr/bin/env sh
printf 'fixture compiler\n'
SH
cat > "$scratch/compiler-objdump" <<'SH'
#!/usr/bin/env sh
printf '42000020 <current()>:\n42000020: 008136 entry a1, 64\n'
SH
chmod +x "$scratch/cache/pio-python/bin/pio" "$scratch/compiler-g++" "$scratch/compiler-objdump"
for attempt in 1 2;do
 ESPSOMFY_TOOL_CACHE="$scratch/cache" STACK_AUDIT_COMPILER="$scratch/compiler-g++" STACK_AUDIT_OUTPUT_ROOT="$scratch/out" \
  bash "$repo/tools/audit-stacks.sh" static
 latest="$(cat "$scratch/out/static/LATEST")"
 python3 - "$latest/usage.json" <<'PY'
import json,sys
from pathlib import Path
r=json.loads(Path(sys.argv[1]).read_text())
assert r['uniqueRepositoryFunctions']==1
assert r['largestFrames'][0]['function']=='void current()'
assert r['auditMode']=='static' and len(r['effectiveConfigSha256'])==64
PY
done
[[ "$(find "$scratch/out/static" -maxdepth 1 -type d -name 'run-*' | wc -l)" == 2 ]]
[[ -f "$scratch/out/static/obsolete/objects/old.su" ]]
printf 'Fresh audit runs exclude stale objects and preserve prior evidence\n'

if ESPSOMFY_TOOL_CACHE="$scratch/cache" STACK_AUDIT_COMPILER="$scratch/compiler-g++" STACK_AUDIT_OUTPUT_ROOT="$scratch/out" \
 bash "$repo/tools/audit-stacks.sh" instrumented > "$scratch/silent-probe.log" 2>&1;then
 echo 'Instrumented firmware without a runtime record format was incorrectly accepted' >&2;exit 1
fi
[[ ! -f "$scratch/out/instrumented/LATEST" ]]
printf 'Silent instrumented image rejected before publication or installation\n'

# Controls require an explicit build mode and a separately emitted marker.
if ESPSOMFY_TOOL_CACHE="$scratch/cache" STACK_AUDIT_COMPILER="$scratch/compiler-g++" STACK_AUDIT_OUTPUT_ROOT="$scratch/out" \
 bash "$repo/tools/audit-stacks.sh" controlled > "$scratch/missing-control.log" 2>&1;then
 echo 'Controlled image missing its runtime interface was accepted' >&2;exit 1
fi
[[ ! -f "$scratch/out/controlled/LATEST" ]]
FAKE_AUDIT_CONTROL=1 ESPSOMFY_TOOL_CACHE="$scratch/cache" STACK_AUDIT_COMPILER="$scratch/compiler-g++" STACK_AUDIT_OUTPUT_ROOT="$scratch/out" \
 bash "$repo/tools/audit-stacks.sh" controlled
control_latest="$(cat "$scratch/out/controlled/LATEST")"
rg -q -- '-DESPSOMFY_AUDIT_CONTROL=1' "$control_latest/platformio.ini"
if FAKE_AUDIT_CONTROL=1 ESPSOMFY_TOOL_CACHE="$scratch/cache" STACK_AUDIT_COMPILER="$scratch/compiler-g++" STACK_AUDIT_OUTPUT_ROOT="$scratch/out" \
 bash "$repo/tools/audit-stacks.sh" instrumented > "$scratch/unexpected-control.log" 2>&1;then
 echo 'Measurement-only mode accepted a control interface' >&2;exit 1
fi
printf 'Explicit control mode and measurement-only exclusion verified\n'

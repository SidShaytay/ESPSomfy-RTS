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

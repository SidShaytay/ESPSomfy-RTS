#!/usr/bin/env bash
# Cross-compile only; never opens the controller, serial port or HA.
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo"
mode="${1:-static}"
case "$mode" in static|instrumented) ;; *) echo 'Usage: bash tools/audit-stacks.sh [static|instrumented]' >&2; exit 2;; esac
cache="${ESPSOMFY_TOOL_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/espsomfy-lilygo}"
pio="$cache/pio-python/bin/pio"
compiler="${STACK_AUDIT_COMPILER:-$HOME/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-g++}"
mode_dir="${STACK_AUDIT_OUTPUT_ROOT:-$repo/build/stack-audit}/$mode"
mkdir -p "$mode_dir"
out="$(mktemp -d "$mode_dir/run-XXXXXXXX")"
python3 - "$repo/platformio.ini" "$out/platformio.ini" "$mode" <<'PY'
from pathlib import Path
import sys
source,out,mode=sys.argv[1:];text=Path(source).read_text()
needle='build_flags =\n\t-DCONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=1'
if text.count(needle)!=1:raise SystemExit('Shared build flags changed; review audit configuration')
flags='\t-fstack-usage\n'
if mode=='instrumented':flags+='\t-DESPSOMFY_STACK_AUDIT=1\n'
Path(out).write_text(text.replace(needle,'build_flags =\n'+flags+'\t-DCONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=1'))
PY
python3 - "$out/source-state.json" "$mode" "$out/platformio.ini" <<'PY'
import json,sys
from pathlib import Path
sys.path.insert(0,'tools')
from stack_usage_report import source_state
state=source_state()
state['auditMode']=sys.argv[2]
state['effectiveConfigSha256']=__import__('hashlib').sha256(Path(sys.argv[3]).read_bytes()).hexdigest()
Path(sys.argv[1]).write_text(json.dumps(state))
PY
export PLATFORMIO_BUILD_DIR="$out/objects"
"$pio" run -c "$out/platformio.ini" -e lilygo-t-embed-cc1101 -j "${ESPSOMFY_BUILD_JOBS:-4}" > "$out/build.log" 2>&1 || { tail -50 "$out/build.log";exit 1; }
python3 - "$PLATFORMIO_BUILD_DIR/lilygo-t-embed-cc1101/firmware.elf" "$PLATFORMIO_BUILD_DIR/lilygo-t-embed-cc1101/firmware.bin" "$mode" <<'PY'
from pathlib import Path
import sys
# Check emitted firmware, not just a successfully compiled probe symbol: release
# log-level filtering can otherwise silently remove every measurement record.
image=Path(sys.argv[2])
if sys.argv[3]=='instrumented':
 if not image.is_file() or image.read_bytes().count(b'STACK_AUDIT schema=1 role=')!=1:
  raise SystemExit('Instrumented image lacks its unique runtime record format')
elif image.is_file() and b'STACK_AUDIT schema=1 role=' in image.read_bytes():
 raise SystemExit('Default image unexpectedly contains stack-audit telemetry')
PY
python3 - "$out/source-state.json" <<'PY'
import json,sys
from pathlib import Path
sys.path.insert(0,'tools')
from stack_usage_report import source_state
before=json.loads(Path(sys.argv[1]).read_text());after=source_state()
if any(after[key]!=before[key] for key in after):raise SystemExit('Source changed during audit build; repeat from a stable source state')
PY
python3 tools/stack_usage_report.py "$PLATFORMIO_BUILD_DIR/lilygo-t-embed-cc1101" \
 --elf "$PLATFORMIO_BUILD_DIR/lilygo-t-embed-cc1101/firmware.elf" --compiler "$compiler" --objdump "${compiler%-g++}-objdump" --source-state "$out/source-state.json" --output "$out/usage.json"
printf '%s\n' "${out#"$repo/"}" > "$mode_dir/LATEST"
printf 'Compiler report: %s/usage.json\n' "$out"

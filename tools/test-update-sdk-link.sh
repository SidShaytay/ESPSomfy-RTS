#!/usr/bin/env bash
# Build/link only. Forces inactive worker+TLS-init code into an ELF, never calls
# it or opens a serial port. Temporary configuration is not a release package.
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cache="${ESPSOMFY_TOOL_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/espsomfy-lilygo}"
pio="${ESPSOMFY_PIO:-$cache/pio-python/bin/pio}"
[[ -x "$pio" ]] || { echo 'Pinned pioarduino Core 6.2 is required.' >&2;exit 1; }
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
cat > "$scratch/link.py" <<'PY'
Import('env')
env.Append(LINKFLAGS=['-Wl,--undefined=_ZN14GitUpdateClass5beginEPF22GitUpdateReadAdmissionvE',
                      '-Wl,--undefined=_Z20gitUpdateMemoryBeginv'])
PY
python3 - "$repo" "$scratch" <<'PY'
from pathlib import Path
import sys
repo,scratch=map(Path,sys.argv[1:])
text=(repo/'platformio.ini').read_text()
anchor='post:archive_elf.py'
if text.count(anchor)!=1:
    raise SystemExit('Expected exactly one archive hook in canonical configuration')
text=text.replace(anchor,anchor+'\n\tpost:'+str(scratch/'link.py'))
(scratch/'platformio.ini').write_text(text)
PY
export PLATFORMIO_BUILD_DIR="${PLATFORMIO_BUILD_DIR:-$repo/.pio/build-update-sdk-link}"
"$pio" run --project-dir "$repo" --project-conf "$scratch/platformio.ini" -e lilygo-t-embed-cc1101 -j "${ESPSOMFY_BUILD_JOBS:-4}"
# No flash/monitor/upload target is involved. Linking itself resolves the real
# framework transport, partition, FreeRTOS and mbedTLS allocator-hook APIs.
printf 'Inactive updater real-SDK link check passed\n'

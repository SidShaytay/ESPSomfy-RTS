#!/usr/bin/env bash
# Offline build/link/package proof only. Never invokes the verifier on a device.
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cache="${ESPSOMFY_TOOL_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/espsomfy-lilygo}"
pio="${ESPSOMFY_PIO:-$cache/pio-python/bin/pio}"
python="$cache/pio-python/bin/python3"
[[ -x "$pio" && -x "$python" ]] || { echo 'Pinned cached pioarduino Core6.2 is required.' >&2;exit 1; }
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
mkdir -p "$repo/build"
out="$(mktemp -d "$repo/build/image-verifier-link.XXXXXX")"
export PLATFORMIO_BUILD_DIR="${PLATFORMIO_BUILD_DIR:-$repo/.pio/build-image-verifier-link}"
[[ "$PLATFORMIO_BUILD_DIR" = /* ]] || PLATFORMIO_BUILD_DIR="$repo/$PLATFORMIO_BUILD_DIR"
export PLATFORMIO_BUILD_DIR
# Exact reviewed32-bit SDK C++ symbol, independently checked as DEFINED below.
symbol='_ZN14GitUpdateImage6verifyEPKhjPKcPKN15GitUpdatePolicy8ManifestERNS_8IdentityE24UpdateAttestationControl'
"$python" - "$repo" "$scratch" "$symbol" <<'PY'
from pathlib import Path
import sys
repo,scratch=map(Path,sys.argv[1:3]);symbol=sys.argv[3]
(scratch/'link.py').write_text("Import('env')\nenv.Append(LINKFLAGS=['-Wl,--undefined="+symbol+"'])\n")
text=(repo/'platformio.ini').read_text();anchor='post:archive_elf.py'
if text.count(anchor)!=1:raise SystemExit('Expected one existing archive hook')
(scratch/'platformio.ini').write_text(text.replace(anchor,anchor+'\n\tpost:'+str(scratch/'link.py')))
PY
"$pio" run --project-dir "$repo" --project-conf "$scratch/platformio.ini" -e lilygo-t-embed-cc1101 -j "${ESPSOMFY_BUILD_JOBS:-4}" 2>&1 | tee "$out/build.log"
"$pio" run --project-dir "$repo" --project-conf "$scratch/platformio.ini" -e lilygo-t-embed-cc1101 -t __idedata > "$out/metadata.log" 2>&1
mkdir "$out/artifacts"
"$python" - "$repo" "$scratch/platformio.ini" "$PLATFORMIO_BUILD_DIR" "$out/artifacts" "$symbol" <<'PY'
from pathlib import Path
import json,os,shutil,subprocess,sys
from platformio.project.config import ProjectConfig
repo,conf,build,out=map(Path,sys.argv[1:5]);symbol=sys.argv[5];env='lilygo-t-embed-cc1101';compiled=build/env
symbols=subprocess.check_output(['readelf','-sW',str(compiled/'firmware.elf')],text=True)
if not any(row.split()[-1:]==[symbol] and ' FUNC ' in row and ' UND ' not in row for row in symbols.splitlines()):
 raise SystemExit('Staged-image verifier was not defined/retained in actual SDK ELF')
for name in ('firmware.bin','firmware.elf','bootloader.bin','partitions.bin','update-source.json'):shutil.copyfile(compiled/name,out/name)
factory=(compiled/'firmware.factory.bin').read_bytes()
if len(factory)<0x10000:raise SystemExit('Incomplete factory staging image')
(out/'boot_app0.bin').write_bytes(factory[0xe000:0x10000])
os.chdir(repo)
(out/'resolved-config.json').write_text(ProjectConfig(str(conf)).to_json())
(out/'compiler-metadata.json').write_text(json.dumps({env:json.loads((compiled/'idedata.json').read_text())}))
sys.path.insert(0,str(repo/'tools'))
from bundled_web_assets import extract
from lilygo_manifest import artifact_manifest,source_record,validate
image=(out/'firmware.bin').read_bytes()
for marker in (b'ESPSOMFY_UPDATE_ID:',b'ESPSOMFY_BOOT_ID:',b'ESPSOMFY_WEB_BUNDLE_V1\0'):
 if image.count(marker)!=1:raise SystemExit('Active verifier adds a duplicate/conflicting compiled sentinel')
extract(image)
manifest=artifact_manifest(out);manifest['source']=source_record(repo);validate(manifest,out)
(out/'flash-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
validate(json.loads((out/'flash-manifest.json').read_text()),out)
PY
UPDATE_IMAGE_FIRMWARE="$out/artifacts/firmware.bin" bash "$repo/tools/test-update-image.sh"
printf 'Actual retained SDK verifier, exact-once sentinels, full package and same-image host verification passed: %s\n' "$out/artifacts"

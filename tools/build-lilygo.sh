#!/usr/bin/env bash
# Build only: this script never opens a serial port or flashes a device.
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cache="${ESPSOMFY_TOOL_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/espsomfy-lilygo}"
source_dir="${1:-$repo}"
out="${2:-$source_dir/build/lilygo}"
source_dir="$(cd -- "$source_dir" && pwd)"
python="$cache/pio-python/bin/python3"
pio="$cache/pio-python/bin/pio"
[[ -x "$pio" && -x "$python" ]] || {
  echo "Run $repo/tools/setup-lilygo.sh first." >&2; exit 1;
}
mkdir -p "$out"
out="$(cd -- "$out" && pwd)"
# Preserve prior output, including unknown caller-owned files. A failed build
# leaves no current artifacts directory that could be mistaken for this build.
if [[ -e "$out/artifacts" || -L "$out/artifacts" ]]; then
  previous="$(mktemp -d "$out/artifacts.previous.XXXXXX")"
  mv -- "$out/artifacts" "$previous/artifacts"
  printf 'Previous package preserved: %s/artifacts\n' "$previous"
fi
staging="$(mktemp -d "$out/.artifacts.XXXXXX")"
trap 'rm -rf -- "$staging"' EXIT
env=lilygo-t-embed-cc1101-plus
# The explicit target list is build-only. No upload/erase/monitor targets.
"$pio" run --project-dir "$source_dir" -e "$env" -j "${ESPSOMFY_BUILD_JOBS:-4}" 2>&1 | tee "$out/build.log"
"$pio" run --project-dir "$source_dir" -e "$env" -t buildfs 2>&1 | tee "$out/filesystem.log"
for image in firmware.bin firmware.elf bootloader.bin partitions.bin littlefs.bin; do
  cp "$source_dir/.pio/build/$env/$image" "$staging/$image"
done
# pioarduino includes OTA initialization in its factory image rather than
# emitting boot_app0.bin separately. Extract only that reserved 8KB segment.
"$python" - "$source_dir/.pio/build/$env/firmware.factory.bin" "$staging/boot_app0.bin" <<'PYCODE'
from pathlib import Path
import sys
factory = Path(sys.argv[1]).read_bytes()
if len(factory) < 0x10000:
    raise SystemExit('Incomplete PlatformIO factory image')
Path(sys.argv[2]).write_bytes(factory[0xe000:0x10000])
PYCODE
# Resolve inheritance/interpolation and collect actual compiler defines/flags
# from PlatformIO, rather than interpreting the INI text ourselves.
"$pio" project config --project-dir "$source_dir" --json-output > "$staging/resolved-config.json"
"$pio" project metadata --project-dir "$source_dir" -e "$env" --json-output-path "$staging/compiler-metadata.json" > "$out/metadata.log" 2>&1
"$python" "$repo/tools/lilygo_manifest.py" create "$staging" --source "$source_dir"
"$python" "$repo/tools/lilygo_manifest.py" validate "$staging"
[[ ! -e "$out/artifacts" && ! -L "$out/artifacts" ]] || {
  echo 'Artifacts destination appeared during build; refusing to overwrite it.' >&2; exit 1;
}
mv -- "$staging" "$out/artifacts"
printf 'Build-only artifacts: %s/artifacts\n' "$out"

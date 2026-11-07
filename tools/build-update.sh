#!/usr/bin/env bash
# Build-only evidence for one registered updater profile. No device access.
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
environment="${1:?Usage: build-update.sh ENVIRONMENT [OUTPUT]}"
out="${2:-$repo/build/$environment}"
cache="${ESPSOMFY_TOOL_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/espsomfy-lilygo}"
python="$cache/pio-python/bin/python3"
pio="$cache/pio-python/bin/pio"
[[ -x "$python" && -x "$pio" ]] || { echo 'Run tools/setup-lilygo.sh first.' >&2; exit 1; }
# Validate the exact environment before using it in a filesystem path or command.
"$python" - "$repo" "$environment" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0,str(Path(sys.argv[1])/'tools'))
from update_profiles import load_profile
if not load_profile(Path(sys.argv[1]),sys.argv[2]):
    raise SystemExit('Environment has no registered verified-update profile')
PY
mkdir -p "$out"
out="$(cd -- "$out" && pwd)"
if [[ -e "$out/artifacts" || -L "$out/artifacts" ]]; then
  previous="$(mktemp -d "$out/artifacts.previous.XXXXXX")"
  mv -- "$out/artifacts" "$previous/artifacts"
  printf 'Previous package preserved: %s/artifacts\n' "$previous"
fi
staging="$(mktemp -d "$out/.artifacts.XXXXXX")"
trap 'rm -rf -- "$staging"' EXIT
"$pio" run --project-dir "$repo" -e "$environment" -j "${ESPSOMFY_BUILD_JOBS:-4}" 2>&1 | tee "$out/build.log"
"$pio" run --project-dir "$repo" -e "$environment" -t buildfs 2>&1 | tee "$out/filesystem.log"
for image in firmware.bin firmware.elf bootloader.bin partitions.bin littlefs.bin update-source.json; do
  cp "$repo/.pio/build/$environment/$image" "$staging/$image"
done
"$pio" project config --project-dir "$repo" --json-output > "$staging/resolved-config.json"
"$pio" project metadata --project-dir "$repo" -e "$environment" --json-output-path "$staging/compiler-metadata.json" > "$out/metadata.log" 2>&1
"$python" "$repo/tools/update_package.py" "$staging" --source "$repo" --environment "$environment"
[[ ! -e "$out/artifacts" && ! -L "$out/artifacts" ]] || { echo 'Artifacts destination appeared during build; refusing overwrite.' >&2; exit 1; }
mv -- "$staging" "$out/artifacts"
printf 'Build-only exact-profile artifacts: %s/artifacts\n' "$out"

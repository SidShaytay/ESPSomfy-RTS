#!/usr/bin/env bash
# Build only: this script never opens a serial port or flashes a device.
# CLI reference: https://arduino.github.io/arduino-cli/1.3/commands/arduino-cli_compile/
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cache="${ESPSOMFY_TOOL_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/espsomfy-lilygo}"
source_dir="${1:-$repo}"
out="${2:-$source_dir/build/lilygo}"
source_dir="$(cd -- "$source_dir" && pwd)"
[[ -x "$cache/bin/arduino-cli" && -x "$cache/build-python/bin/python3" ]] || {
  echo "Run $repo/tools/setup-lilygo.sh first." >&2; exit 1;
}
mkdir -p "$out"
rm -f -- "$out/artifacts/flash-manifest.json"
out="$(cd -- "$out" && pwd)"
# Retain default OTA/LittleFS offsets for compatibility with ESPSomfy's updater.
# The remaining 12MB of the board's 16MB flash is intentionally unallocated.
fqbn='esp32:esp32:esp32s3:PSRAM=opi,FlashMode=qio,FlashSize=16M,LoopCore=1,EventsCore=1,USBMode=hwcdc,CDCOnBoot=cdc,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,PartitionScheme=default,CPUFreq=240,UploadSpeed=921600,DebugLevel=none,EraseFlash=none'
export PATH="$cache/build-python/bin:$PATH"
stage="$(mktemp -d "$out/sketch.XXXXXX")"
trap 'rm -rf -- "$stage"' EXIT
mkdir "$stage/SomfyController"
cp "$source_dir/"*.ino "$source_dir/"*.cpp "$source_dir/"*.h "$stage/SomfyController/"
cp -R "$source_dir/data" "$stage/SomfyController/data"
"$cache/bin/arduino-cli" --config-file "$cache/arduino-cli.yaml" compile \
  --clean --jobs "${ESPSOMFY_BUILD_JOBS:-4}" --fqbn "$fqbn" --warnings default \
  --build-property compiler.cpp.extra_flags=-DESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS \
  --build-path "$out/objects" --output-dir "$out/artifacts" "$stage/SomfyController" 2>&1 | tee "$out/build.log"
"$cache/bin/mklittlefs" --create "$stage/SomfyController/data" --size 1441792 "$out/artifacts/SomfyController.littlefs.bin"
cp "$cache/arduino-data/packages/esp32/hardware/esp32/2.0.17/tools/partitions/boot_app0.bin" "$out/artifacts/boot_app0.bin"
python3 - "$source_dir" "$out/artifacts" "$fqbn" "$stage/SomfyController" <<'PY'
import hashlib, json, pathlib, struct, subprocess, sys
source, out = map(pathlib.Path, sys.argv[1:3])
partitions = {}
for row in struct.iter_unpack('<HBBLL16sL', (out / 'SomfyController.ino.partitions.bin').read_bytes()):
    magic, kind, subtype, offset, size, name, flags = row
    if magic == 0x50AA:
        partitions[name.rstrip(b'\0').decode()] = {'offset': offset, 'size': size}
expected = {'nvs': (0x9000, 0x5000), 'otadata': (0xe000, 0x2000), 'app0': (0x10000, 0x140000), 'app1': (0x150000, 0x140000), 'spiffs': (0x290000, 0x160000)}
for name, (offset, size) in expected.items():
    if partitions.get(name) != {'offset': offset, 'size': size}:
        raise SystemExit(f'Unexpected {name} partition; refusing to produce flash manifest')
segments = []
for offset, filename, limit in [(0, 'SomfyController.ino.bootloader.bin', 0x8000), (0x8000, 'SomfyController.ino.partitions.bin', 0x1000), (0xe000, 'boot_app0.bin', 0x2000), (0x10000, 'SomfyController.ino.bin', 0x140000), (0x290000, 'SomfyController.littlefs.bin', 0x160000)]:
    data = (out / filename).read_bytes()
    if len(data) > limit:
        raise SystemExit(f'{filename} exceeds its partition')
    segments.append({'offset': hex(offset), 'file': filename, 'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
revision = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
staged = pathlib.Path(sys.argv[4])
source_hashes = {str(p.relative_to(staged)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(staged.rglob('*')) if p.is_file()}
manifest = {'source_sha256': source_hashes, 'board': 'LILYGO T-Embed CC1101 Plus', 'chip': 'esp32s3', 'flash_size': '16MB', 'fqbn': sys.argv[3], 'source_revision': revision, 'arduino_core': '2.0.17', 'partitions': partitions, 'segments': segments}
(out / 'flash-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(f'Artifacts: {out}\nFlash offsets and SHA256: {out / "flash-manifest.json"}')
PY

#!/usr/bin/env bash
# Installs the release workflow's toolchain in a user-owned cache (Linux x86_64).
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cache="${ESPSOMFY_TOOL_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/espsomfy-lilygo}"
[[ "$(uname -sm)" == 'Linux x86_64' ]] || { echo 'This setup supports Linux x86_64.' >&2; exit 1; }
command -v uv >/dev/null || { echo 'Install uv before setting up this toolchain.' >&2; exit 1; }
mkdir -p "$cache/bin" "$cache/downloads"
if [[ ! -x "$cache/build-python/bin/python3" ]]; then
  uv venv --python 3.11.14 "$cache/build-python"
fi
uv pip sync --python "$cache/build-python/bin/python3" "$repo/tools/lilygo-python-requirements.txt"
fetch() {
  local url="$1" file="$cache/downloads/$2" digest="$3"
  if [[ ! -f "$file" ]]; then
    curl --fail --location --retry 3 "$url" --output "$file.part"
    mv "$file.part" "$file"
  fi
  printf '%s  %s\n' "$digest" "$file" | sha256sum --check --status
}
# Official release assets; pin both versions and downloaded bytes.
fetch 'https://github.com/arduino/arduino-cli/releases/download/v1.3.1/arduino-cli_1.3.1_Linux_64bit.tar.gz' \
  arduino-cli_1.3.1_Linux_64bit.tar.gz 376428d7d45be640c00812a71612e1742edc2f5f9ee3742a2d6da7870e079588
tar -xzf "$cache/downloads/arduino-cli_1.3.1_Linux_64bit.tar.gz" -C "$cache/bin" arduino-cli
fetch 'https://github.com/earlephilhower/mklittlefs/releases/download/3.1.0/x86_64-linux-gnu-mklittlefs-4aca452.tar.gz' \
  mklittlefs-3.1.0.tar.gz a91e2425575d9132b44b577059c669d023d43647331feb742990045ec7f8df08
tar -xzf "$cache/downloads/mklittlefs-3.1.0.tar.gz" -C "$cache/downloads" mklittlefs/mklittlefs
cp "$cache/downloads/mklittlefs/mklittlefs" "$cache/bin/mklittlefs"
# mklittlefs 3.1.0 bundles LittleFS v2.5.1, matching release.yaml.
python3 - "$cache" <<'PY'
import json, pathlib, sys
cache = pathlib.Path(sys.argv[1]).resolve()
config = {
    'board_manager': {'additional_urls': ['https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json']},
    'directories': {'data': str(cache / 'arduino-data'), 'downloads': str(cache / 'downloads'), 'user': str(cache / 'arduino-user')},
}
(cache / 'arduino-cli.yaml').write_text(json.dumps(config, indent=2) + '\n')
PY
cli=("$cache/bin/arduino-cli" --config-file "$cache/arduino-cli.yaml")
"${cli[@]}" core update-index
"${cli[@]}" core install esp32:esp32@2.0.17
"${cli[@]}" lib install --no-deps ArduinoJson@6.21.5 PubSubClient@2.8.0 SmartRC-CC1101-Driver-Lib@2.5.7 WebSockets@2.4.0
"${cli[@]}" version
"$cache/bin/mklittlefs" --version

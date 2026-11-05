#!/usr/bin/env bash
# Build tools only, installed in a user-owned cache. Never uses sudo or a device.
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cache="${ESPSOMFY_TOOL_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/espsomfy-lilygo}"
command -v uv >/dev/null || { echo 'Install uv before setting up this toolchain.' >&2; exit 1; }
mkdir -p "$cache"
if [[ ! -x "$cache/pio-python/bin/python3" ]]; then
  uv venv --python 3.12.12 "$cache/pio-python"
fi
uv pip sync --python "$cache/pio-python/bin/python3" "$repo/tools/lilygo-python-requirements.txt"
"$cache/pio-python/bin/pio" --version
printf 'Build command: %s/tools/build-lilygo.sh\n' "$repo"
printf 'Host native tests additionally require gcc/g++ on PATH.\n'

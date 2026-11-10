#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
python3 -m unittest discover -s "$repo/tools" -p test_bundled_web.py
exec bash "$repo/tools/test-bundled-web.sh"

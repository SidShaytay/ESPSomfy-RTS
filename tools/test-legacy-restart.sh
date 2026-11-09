#!/usr/bin/env bash
# The 32-bit legacy timer was retired; exercise its real service replacement.
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
exec bash "$repo/tools/test-safe-restart.sh"

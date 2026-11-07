#!/usr/bin/env bash
# Host only: scripted HTTPS/FreeRTOS/OTA SDK boundaries; no controller access.
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
exec "${PYTHON:-python3}" "$repo/tests/git-update-worker/run.py"

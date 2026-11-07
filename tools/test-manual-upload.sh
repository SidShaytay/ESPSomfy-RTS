#!/usr/bin/env bash
# Host-only multipart and update ownership regression tests; no device access.
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
output="$(mktemp -d)"
trap 'rm -rf -- "$output"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I"$repo/src" "$repo/tests/manual-upload/lifecycle.cpp" -o "$output/lifecycle"
"$output/lifecycle"

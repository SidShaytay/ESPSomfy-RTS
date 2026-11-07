#!/usr/bin/env bash
# Host-only transaction checks. Never contacts the controller.
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
output="$(mktemp -d)"
trap 'rm -rf -- "$output"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I"$repo/src" "$repo/tests/restore-upload/lifecycle.cpp" -o "$output/lifecycle"
"$output/lifecycle"

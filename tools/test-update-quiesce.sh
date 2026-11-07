#!/usr/bin/env bash
# Host-only producer/snapshot concurrency checks.
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread -I"$repo/src" \
  "$repo/tests/update-quiesce/queue-test.cpp" -o "$scratch/queue-test"
"$scratch/queue-test"
echo 'Update queue admission checks passed'
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I"$repo/src" \
  "$repo/tests/update-quiesce/radio-test.cpp" -o "$scratch/radio-test"
"$scratch/radio-test"
echo 'Update radio pending-work checks passed'
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread -I"$repo/src" \
  "$repo/tests/update-quiesce/reboot-test.cpp" -o "$scratch/reboot-test"
"$scratch/reboot-test"
echo 'Wrap-safe reboot deadline checks passed'

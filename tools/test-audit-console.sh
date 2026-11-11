#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)";trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I"$repo/src" "$repo/tests/audit-console/protocol.cpp" -o "$scratch/protocol"
"$scratch/protocol"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -DESPSOMFY_STACK_AUDIT -DESPSOMFY_AUDIT_CONTROL -DAUDIT_CONSOLE_HOST_TEST -I"$repo/src" "$repo/tests/audit-console/runtime.cpp" -o "$scratch/runtime"
"$scratch/runtime"
printf '#include "AuditConsole.h"\nint main(){AuditConsole::loop();}\n' > "$scratch/disabled.cpp"
"${CXX:-c++}" -std=c++17 -I"$repo/src" "$scratch/disabled.cpp" "$repo/src/AuditConsole.cpp" -o "$scratch/disabled"
"$scratch/disabled"

#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)";trap 'rm -rf -- "$scratch"' EXIT
for log_level in 2 3;do
"${CXX:-c++}" -std=c++17 -O2 -DESPSOMFY_STACK_AUDIT=1 -DCORE_DEBUG_LEVEL="$log_level" -I"$repo/tests/stack-audit/stubs" -I"$repo/src" \
 "$repo/tests/stack-audit/test.cpp" "$repo/src/StackAudit.cpp" -o "$scratch/test"
"$scratch/test"
done
for sdk_case in sdk-test sdk-startup-test;do
"${CXX:-c++}" -std=c++17 -O2 -DESPSOMFY_STACK_AUDIT=1 -I"$repo/tests/stack-audit/stubs" -I"$repo/src" \
 "$repo/tests/stack-audit/$sdk_case.cpp" "$repo/src/StackAudit.cpp" -o "$scratch/sdk"
"$scratch/sdk"
done
# The default production build must not acquire RTOS/logging dependencies.
printf '#include "StackAudit.h"\nint main(){StackAudit::Scope scope;StackAudit::beginSdk();StackAudit::pollSdk();}\n' > "$scratch/disabled.cpp"
"${CXX:-c++}" -std=c++17 -O2 -I"$repo/src" "$scratch/disabled.cpp" "$repo/src/StackAudit.cpp" -o "$scratch/disabled"
"$scratch/disabled"

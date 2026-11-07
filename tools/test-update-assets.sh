#!/usr/bin/env bash
# Host-only asset verification. Never contacts the controller.
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
output="$(mktemp -d)"
trap 'rm -rf -- "$output"' EXIT
python3 - "$repo" "$output" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0,str(Path(sys.argv[1])/'tools'))
from update_assets import write_manifest
out=Path(sys.argv[2]);data=out/'data';data.mkdir()
(data/'index.js.gz').write_bytes(b'packed\0web javascript'*250)
(data/'appversion').write_text('4.0.1\n')
(data/'empty.css').write_bytes(b'')
compat=write_manifest(data)
(out/'compat.h').write_text('#define ESPSOMFY_APP_COMPAT "'+compat+'"\n')
PY
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -DESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS -include "$output/compat.h" -I"$repo/tests/update-assets" -I"$repo/test/stubs" -I"$repo/src" "$repo/src/GitUpdateAssets.cpp" "$repo/tests/update-assets/runtime.cpp" -o "$output/runtime"
"$output/runtime" "$output/data"

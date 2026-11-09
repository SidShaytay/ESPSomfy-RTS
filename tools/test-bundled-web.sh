#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
compat=$(python3 - "$repo" "$scratch" <<'PY'
import gzip,sys
from pathlib import Path
sys.path.insert(0,str(Path(sys.argv[1])/'tools'))
from update_assets import write_manifest
from bundled_web_assets import generate
root=Path(sys.argv[2]);data=root/'data';data.mkdir()
for name,content in [('index.html','<html>test</html>'),('index.js','window.test=1;'),('app.css','body{}')]:
 (data/(name+'.gz')).write_bytes(gzip.compress(content.encode(),mtime=0))
(data/'appversion').write_bytes(b'4.0.1\n')
write_manifest(data)
print(generate(data,root/'BundledWebData.h')['compatibility'])
PY
)
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -DESPSOMFY_BUNDLED_WEB=1 -DESPSOMFY_APP_COMPAT=\""$compat"\" \
 -I"$scratch" -I"$repo/tests/bundled-web" -I"$repo/tests/update-assets" -I"$repo/src" -I"$repo/test/stubs" \
 "$repo/tests/bundled-web/http-test.cpp" "$repo/src/BundledWeb.cpp" "$repo/src/entryPoints/BundledWebRoutes.cpp" -o "$scratch/http"
"$scratch/http"

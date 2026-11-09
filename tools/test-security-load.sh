#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
json="${ARDUINOJSON_INCLUDE:-$repo/.pio/libdeps/native/ArduinoJson/src}"
python3 - "$repo" "$scratch" <<'PY'
from pathlib import Path
import sys
root,scratch=map(Path,sys.argv[1:]);text=(root/'src/entryPoints/Web.cpp').read_text();out=[]
for name in ['Web::tokenAccepted','Web::isAuthenticated','Web::handleLogin']:
 at=text.index(name+'(');start=text.rfind('\n',0,at)+1;brace=text.index('{',at);end=brace+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 out.append(text[start:end])
for filename,name in [('src/entryPoints/HttpRoutes.cpp','uploadAllowed'),('src/outputs/ProtoSocket.cpp','authorized')]:
 text=(root/filename).read_text();at=text.index(name+'(');start=text.rfind('\n',0,at)+1;brace=text.index('{',at);end=brace+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 out.append(text[start:end])
(scratch/'production-web-auth.inc').write_text('\n'.join(out))
text=(root/'src/SomfyController.cpp').read_text();start=text.index('  bool updateNetworkReady=');end=text.index('  esp_task_wdt_reset();',start)
body=text[start:end]
utils=(root/'src/Utils.cpp').read_text();start=utils.index('bool otaProven(');brace=utils.index('{',start);end=utils.index('}',brace)+1
(scratch/'production-security-health.inc').write_text(utils[start:end]+'\nvoid health(){\n'+body+'\n}\n')

PY
"${CXX:-c++}" -std=c++17 -O2 -I"$scratch" -I"$json" -I"$repo/test/stubs" -I"$repo/src" "$repo/tests/security-load/web-auth-test.cpp" "$repo/src/SecurityLoad.cpp" "$repo/src/SecuritySettingsLoad.cpp" -o "$scratch/auth"
"$scratch/auth"
"${CXX:-c++}" -std=c++17 -O2 -DESPSOMFY_LOCAL_UI -I"$repo/tests/ui-adapter" -I"$repo/test/stubs" -I"$repo/src" "$repo/tests/ui-adapter/authorization-test.cpp" -o "$scratch/lcd"
"$scratch/lcd"

"${CXX:-c++}" -std=c++17 -O2 -I"$scratch" "$repo/tests/security-load/health-test.cpp" -o "$scratch/health"
"$scratch/health"

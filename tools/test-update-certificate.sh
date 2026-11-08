#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -pthread -DGIT_UPDATE_CERTIFICATE_HOST_TEST -Wall -Wextra -Werror \
  -I"$repo/tests/update-certificate" -I"$repo/src" \
  "$repo/tests/update-certificate/certificate-test.cpp" "$repo/src/GitUpdateCertificate.cpp" -o "$scratch/test"
"$scratch/test"
printf 'Updater certificate date/context checks passed\n'
# Version pins and private-field types are part of this adapter's contract.
for changed in idf mbed callback configuration padding; do
  cp -r -- "$repo/tests/update-certificate" "$scratch/$changed"
  python3 - "$scratch/$changed" "$changed" <<'PYCASE'
from pathlib import Path
import sys
root,changed=Path(sys.argv[1]),sys.argv[2]
if changed=='idf':
    p=root/'esp_idf_version.h';p.write_text(p.read_text().replace('ESP_IDF_VERSION_VAL(5,5,5)','ESP_IDF_VERSION_VAL(5,5,4)'))
elif changed=='mbed':
    p=root/'mbedtls/build_info.h';p.write_text(p.read_text().replace('0x03060600','0x03060500'))
elif changed=='padding':
    p=root/'mbedtls/ssl.h';p.write_text(p.read_text().replace('struct mbedtls_ssl_config {','struct mbedtls_ssl_config { int leading;'))
elif changed=='configuration':
    p=root/'mbedtls/build_info.h';p.write_text(p.read_text().replace('mbedtls/esp_config.h','mbedtls/mbedtls_config.h'))
else:
    p=root/'mbedtls/ssl.h';p.write_text(p.read_text().replace('*,int,uint32_t *)=nullptr','*,long,uint32_t *)=nullptr'))
PYCASE
  if "${CXX:-c++}" -std=c++17 -DGIT_UPDATE_CERTIFICATE_HOST_TEST \
      -I"$scratch/$changed" -I"$repo/src" -c "$repo/src/GitUpdateCertificate.cpp" \
      -o "$scratch/rejected.o" > "$scratch/rejected.log" 2>&1; then
    echo "Unreviewed certificate adapter contract accepted: $changed" >&2;exit 1
  fi
done
printf 'Unreviewed SDK/version/callback contracts rejected\n'

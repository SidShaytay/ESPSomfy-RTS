"""Run production singleton worker with entirely scripted local boundaries."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
include=Path(os.environ.get('ARDUINOJSON_INCLUDE',root/'.pio/libdeps/native/ArduinoJson/src'))
if not (include/'ArduinoJson.h').is_file():
    raise SystemExit('Run native tests first or set ARDUINOJSON_INCLUDE to the pinned cache.')
compiler=shlex.split(os.environ.get('CXX','g++'))
common=compiler+['-pthread','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(include),'-I'+str(root/'src'),'-I'+str(root/'tests/update-certificate')]
with tempfile.TemporaryDirectory(prefix='espsomfy-worker-') as scratch:
    binary=Path(scratch)/'worker'
    shared=common+['-DESPSOMFY_LILYGO_T_EMBED_CC1101','-DGIT_UPDATE_HOST_TEST',str(root/'tests/git-update-worker/worker-test.cpp'),str(root/'src/GitUpdatePolicy.cpp'),str(root/'src/GitUpdateCertificate.cpp'),'-Wl,--wrap=time','-l:libcrypto.so.3','-o',str(binary)]
    subprocess.run(shared,check=True)
    for case in ('entropyfail','reboot','valid','prepare','changedsource','changedimage','changedmanifest','changedversion','generation','failedrefresh',
        'admission','lowmem','fragmented','clock','boot','assets','layout','wrongrepo','draft','prerelease','nul','types','features','boards','oldschema','huge','digest',
        'chunkapi','chunkmanifest','truncated','unclean','encoding','redirect','redirectbad','redirectloop','cancel','cancelattest','timeout','allocation','memorynotready','createfail','notifyfail','allocationpartial','metadata_budget','duplicateassets','missingdigest','boolfeatures','floatnumeric','notpublished',
        'duplicatelocation','longlocation','redirectrepo','sameversion','downgrade','generationlarge','generationmax','externalinvalidates','wrapclock','expiredcert','futurecert','expiredintermediate','missingverify','bundlefailure'):
        subprocess.run([str(binary),case],check=True)
    dev=shared.copy();dev.insert(len(common),'-DESPSOMFY_RELEASE_TAG="dev-source"');subprocess.run(dev,check=True)
    subprocess.run([str(binary),'dev'],check=True)
    subprocess.run(common+[str(root/'tests/git-update-worker/ownership-test.cpp'),str(root/'src/GitUpdate.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)

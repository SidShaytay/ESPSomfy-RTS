"""Offline real-worker boundary regression; no controller or network transport."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
json = Path(os.environ.get('ARDUINOJSON_INCLUDE', root / '.pio/libdeps/native/ArduinoJson/src'))
compiler = shlex.split(os.environ.get('CXX', 'c++'))
with tempfile.TemporaryDirectory(prefix='espsomfy-install-') as scratch:
    binary = Path(scratch) / 'install'
    command = compiler + ['-std=c++17', '-pthread', '-Wall', '-Wextra', '-Werror',
        '-DESPSOMFY_LILYGO_T_EMBED_CC1101', '-DGIT_UPDATE_HOST_TEST']
    command += ['-I' + str(path) for path in (json, root / 'src',
        root / 'tests/update-certificate', root / 'tests/update-assets', root / 'test/stubs')]
    command += [str(root / path) for path in ('tests/git-update-install/install-test.cpp',
        'src/GitUpdatePolicy.cpp', 'src/GitUpdateCertificate.cpp',
        'src/GitUpdateImage.cpp', 'src/UpdateSha256.cpp')]
    subprocess.run(command + ['-Wl,--wrap=time', '-l:libcrypto.so.3', '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('Matching digest with invalid firmware: no preparation, flash write or boot selection')

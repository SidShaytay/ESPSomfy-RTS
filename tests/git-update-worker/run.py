#!/usr/bin/env python3
"""Compile the actual production worker separately for every reviewed profile."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
profile_source = Path(os.environ.get('UPDATE_PROFILE_SOURCE', root)).resolve()
sys.path.insert(0, str(profile_source / 'tools'))
from update_profiles import profile_defines, supported_profiles

json_include = Path(os.environ.get('ARDUINOJSON_INCLUDE', root / '.pio/libdeps/native/ArduinoJson/src'))
if not json_include.is_dir():
    sys.exit('Run pio test -e native first to install pinned ArduinoJson.')
compiler = shlex.split(os.environ.get('CXX', 'g++'))
with tempfile.TemporaryDirectory(prefix='github-worker-profiles-') as temporary:
    for environment, profile in supported_profiles(profile_source).items():
        defines = ['-DESPSOMFY_GITHUB_UPDATES', '-DGIT_UPDATE_HOST_TEST']
        for macro, value in profile_defines(profile):
            defines.append('-D' + macro + '=' + (json.dumps(value) if isinstance(value, str) else str(value)))
        binary = Path(temporary) / environment
        subprocess.run(compiler + ['-std=c++17', '-Wall', '-Wextra', '-Werror',
            '-I' + str(json_include), '-I' + str(root / 'src'), *defines,
            str(root / 'tests/git-update-worker/worker-test.cpp'), str(root / 'src/GitUpdatePolicy.cpp'),
            '-Wl,--wrap=time', '-l:libcrypto.so.3', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)

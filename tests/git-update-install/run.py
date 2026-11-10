"""Offline real-worker boundary regression; no controller or network transport."""
import gzip
import hashlib
import struct
import sys
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
json = Path(os.environ.get('ARDUINOJSON_INCLUDE', root / '.pio/libdeps/native/ArduinoJson/src'))
sys.path.insert(0, str(root / 'tools'))
from update_assets import write_manifest
from bundled_web_assets import build_blob
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
    data = Path(scratch) / 'data'
    data.mkdir()
    for name, body in [('index.html', '<html><meta name="espsomfy-web-mode" content="bundled"></html>'), ('index.js', 'window.test=1'), ('app.css', 'body{}')]:
        (data / (name + '.gz')).write_bytes(gzip.compress(body.encode(), mtime=0))
    (data / 'appversion').write_bytes(b'4.0.1')
    compat = write_manifest(data)
    marker = ('ESPSOMFY_UPDATE_ID:3|lilygo-t-embed-cc1101|default_8MB|' + 'a'*40 + '|v4.0.3|bundled|' + compat).encode() + b'\0'
    boot = ('ESPSOMFY_BOOT_ID:0:256:' + 'b'*64).encode() + b'\0'
    desc = bytearray(256)
    struct.pack_into('<I', desc, 0, 0xabcd5432)
    desc[112:120] = b'v5.5.5\0\0'
    payload = bytes(desc) + marker + boot + build_blob(data) + b'\0'*65536
    header = bytearray(24)
    header[0:4] = bytes([0xe9, 1, 2, 0x4f])
    struct.pack_into('<H', header, 12, 9)
    header[23] = 1
    def make_image(segment):
        image = bytes(header) + struct.pack('<II', 0x3c000020, len(segment)) + segment
        checksum = 0xef
        for value in segment:
            checksum ^= value
        image += b'\0'*((15-len(image)%16)%16) + bytes([checksum])
        return image + hashlib.sha256(image).digest()
    image = make_image(payload)
    firmware = Path(scratch) / 'firmware.bin'
    firmware.write_bytes(image)
    for mode in ('invalid', 'valid', 'cancelWrite', 'abortFailure', 'cleanupBusy', 'sourcePrepare', 'notifyFail', 'beginFail', 'endFail', 'writeFail',
                 'notifyWrite', 'allocationWrite', 'cancelBegin', 'cancelEnd', 'expireWrite', 'expireEnd', 'sourceWrite', 'cancelStage', 'refuseAdmission', 'shortImage',
                 'activateOkay', 'activateFailure', 'activateWrongSlot', 'activateWrongSubtype', 'activateBadReadback'):
        subprocess.run([str(binary), mode, str(firmware), compat], check=True)
        print('PASS install ' + mode, flush=True)
    coupled = command[:len(compiler)] + ['-DGIT_UPDATE_CONTROLLER_TEST', '-DESPSOMFY_LOCAL_UI', '-I'+str(root/'tests/git-update-controller')] + command[len(compiler):]
    coupled += [str(root/'src'/name) for name in ('RestartCoordinator.cpp', 'RestartPolicy.cpp', 'SettingsDurability.cpp', 'SomfyFrame.cpp')]
    subprocess.run(coupled + ['-Wl,--wrap=time', '-l:libcrypto.so.3', '-o', str(binary)], check=True)
    for mode in ('inactivityStage','deadlineStage','valid', 'moving', 'queued', 'localPending', 'radioWork', 'pauseFailure', 'saveFailure', 'sourceSave', 'configSave', 'cancelWrite', 'sourceWrite', 'cleanupBusy', 'activationFailure', 'expireFinal', 'cancelFinal', 'staleCleanup',
                 'manualValid', 'manualReserved', 'manualReservationFailure', 'manualInvalid', 'manualOldAssets', 'manualWrongGrant', 'manualExpiredGrant', 'manualWriteFailure', 'manualBadMemory', 'manualNoClaim', 'manualLateAdoption', 'manualNotifyFailure'):
        subprocess.run([str(binary), mode, str(firmware), compat], check=True)
        print('PASS controller ' + mode, flush=True)
    developer=Path(scratch)/'developer.bin'
    developer.write_bytes(make_image(payload.replace(b'|v4.0.3|', b'|dev-'+b'a'*12+b'|', 1)))
    subprocess.run([str(binary), 'manualValid', str(developer), compat], check=True)
    dev_command=coupled+['-DESPSOMFY_RELEASE_TAG="dev-aaaaaaaaaaaa"']
    subprocess.run(dev_command+['-Wl,--wrap=time', '-l:libcrypto.so.3', '-o', str(binary)], check=True)
    subprocess.run([str(binary), 'manualValid', str(developer), compat], check=True)
    print('PASS manual developer file from stable and developer installed builds', flush=True)

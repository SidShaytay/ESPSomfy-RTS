"""Download-only audit uses the real worker, HTTPS boundary and staged verifier."""
import gzip,hashlib,os,shlex,struct,subprocess,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
include=Path(os.environ.get('ARDUINOJSON_INCLUDE',root/'.pio/libdeps/native/ArduinoJson/src'))
sys.path.insert(0,str(root/'tools'))
from update_assets import write_manifest
from bundled_web_assets import build_blob
compiler=shlex.split(os.environ.get('CXX','g++'))
with tempfile.TemporaryDirectory(prefix='audit-stage-') as scratch:
    p=Path(scratch);binary=p/'audit-stage'
    common=compiler+['-std=c++17','-pthread','-Wall','-Wextra','-Werror','-DESPSOMFY_LILYGO_T_EMBED_CC1101','-DGIT_UPDATE_HOST_TEST']
    common+=['-I'+str(q) for q in (include,root/'src',root/'tests/update-certificate',root/'tests/update-assets',root/'test/stubs')]
    sources=[str(root/q) for q in ('tests/git-update-worker/audit-stage-test.cpp','src/GitUpdatePolicy.cpp','src/GitUpdateCertificate.cpp','src/GitUpdateImage.cpp','src/UpdateSha256.cpp')]
    flags=['-DESPSOMFY_STACK_AUDIT','-DESPSOMFY_AUDIT_CONTROL']
    subprocess.run(common+flags+sources+['-Wl,--wrap=time','-l:libcrypto.so.3','-o',str(binary)],check=True)
    data=p/'data';data.mkdir()
    for name,body in [('index.html','<html><meta name="espsomfy-web-mode" content="bundled"></html>'),('index.js','window.test=1'),('app.css','body{}')]:
        (data/(name+'.gz')).write_bytes(gzip.compress(body.encode(),mtime=0))
    (data/'appversion').write_bytes(b'4.0.1');compat=write_manifest(data)
    def image(tag):
        marker=('ESPSOMFY_UPDATE_ID:3|lilygo-t-embed-cc1101|default_8MB|'+'a'*40+'|'+tag+'|bundled|'+compat).encode()+b'\0'
        boot=('ESPSOMFY_BOOT_ID:0:256:'+'b'*64).encode()+b'\0'
        desc=bytearray(256);struct.pack_into('<I',desc,0,0xabcd5432);desc[112:120]=b'v5.5.5\0\0'
        payload=bytes(desc)+marker+boot+build_blob(data)+b'\0'*65536
        header=bytearray(24);header[0:4]=bytes([0xe9,1,2,0x4f]);struct.pack_into('<H',header,12,9);header[23]=1
        result=bytes(header)+struct.pack('<II',0x3c000020,len(payload))+payload
        checksum=0xef
        for byte in payload:checksum^=byte
        result+=b'\0'*((15-len(result)%16)%16)+bytes([checksum])
        return result+hashlib.sha256(result).digest()
    for mode in ('valid','same','older','changedsource','changedmanifest','transport','invalidimage','cancel','cancelverify','admission','notifyfail','idletimeout'):
        path=p/'firmware.bin';path.write_bytes(image('v4.0.2' if mode=='same' else 'v4.0.1' if mode=='older' else 'v4.0.3'))
        subprocess.run([str(binary),mode,str(path),compat],check=True)
    # A regular measurement-only build must not expose the controlled API.
    default=common+[str(root/'tests/git-update-worker/worker-test.cpp')]+sources[1:]+['-DESPSOMFY_STACK_AUDIT','-Wl,--wrap=time','-l:libcrypto.so.3','-o',str(binary)]
    subprocess.run(default,check=True)
    symbols=subprocess.check_output(['nm','-C',str(binary)],text=True)
    assert 'requestAuditStage' not in symbols and 'stageAudit' not in symbols
    print('Audit stage API absent from ordinary measurement-only build')

"""Real staged-image verifier; local synthetic ESP images and independent SHA oracle."""
import gzip,hashlib,os,re,shlex,struct,subprocess,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'tools'))
from update_assets import write_manifest
from bundled_web_assets import build_blob,MAGIC
compiler=shlex.split(os.environ.get('CXX','g++'))
with tempfile.TemporaryDirectory(prefix='espsomfy-images-') as temporary:
 p=Path(temporary);data=p/'data';data.mkdir()
 for name,body in [('index.html','<html><meta name="espsomfy-web-mode" content="bundled"></html>'),('index.js','window.test=1'),('app.css','body{}')]:
  (data/(name+'.gz')).write_bytes(gzip.compress(body.encode(),mtime=0))
 (data/'appversion').write_bytes(b'4.0.1');compat=write_manifest(data);blob=build_blob(data)
 marker=('ESPSOMFY_UPDATE_ID:3|lilygo-t-embed-cc1101|default_8MB|'+'a'*40+'|v4.0.3|bundled|'+compat).encode()+b'\0'
 boot=('ESPSOMFY_BOOT_ID:0:24576:'+'b'*64).encode()+b'\0'
 desc=bytearray(256);struct.pack_into('<I',desc,0,0xabcd5432);desc[112:120]=b'v5.5.5\0\0'
 def image(contents,description=desc):
  payload=bytes(description)+contents+b'\0'*1024
  header=bytearray(24);header[0:4]=bytes([0xe9,1,2,0x4f]);struct.pack_into('<H',header,12,9);header[23]=1
  out=bytes(header)+struct.pack('<II',0x3c000020,len(payload))+payload
  checksum=0xef
  for b in payload:checksum^=b
  out+=b'\0'*((15-len(out)%16)%16)+bytes([checksum]);return out+hashlib.sha256(out).digest()
 good=image(marker+boot+blob)
 binary=p/'test'
 subprocess.run(compiler+['-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(root/'src'),'-I'+str(root/'tests/update-assets'),'-I'+str(root/'test/stubs'),str(root/'tests/update-image/image-test.cpp'),str(root/'src/GitUpdateImage.cpp'),str(root/'src/UpdateSha256.cpp'),str(root/'src/GitUpdatePolicy.cpp'),'-o',str(binary)],check=True)
 def run(mode,firmware=good,yes=False):
  path=p/'firmware.bin';path.write_bytes(firmware)
  subprocess.run([str(binary),mode,str(path),hashlib.sha256(firmware).hexdigest(),compat,'yes' if yes else 'no'],check=True)
 for mode in ('valid','manual'):run(mode,yes=True)
 dev=marker.replace(b'|v4.0.3|',b'|dev-aaaaaaaaaaaa-dirty|');run('manualDev',image(dev+boot+blob),True)
 for mode in ('wrongExpectedDigest','wrongExpectedWeb','wrongExpectedVersion','wrongExpectedSource','wrongExpectedSize','startHash','updateHash','finishHash','cancel','cancelLate','cancelFinish','cancelFree'):run(mode)
 for name,replacement in [('schema',marker.replace(b'ID:3|',b'ID:2|')),('retired',marker.replace(b'cc1101|',b'cc1101-plus|')),('mode',marker.replace(b'|bundled|',b'|filesystem|')),('source',marker.replace(b'a'*40,b'g'*40)),('duplicate',marker+marker),('missing',b''),('leadingVersion',marker.replace(b'v4.0.3',b'v04.0.3')),('overflowVersion',marker.replace(b'v4.0.3',b'v4294967296.0.0')),('badDev',marker.replace(b'v4.0.3',b'dev-bbbbbbbbbbbb'))]:run('manual:'+name,image(replacement+boot+blob))
 for name,replacement in [('missingBoot',b''),('duplicateBoot',boot+boot),('wrongBoot',boot.replace(b'b'*64,b'c'*64)),('bootOffset',boot.replace(b'ID:0:',b'ID:4096:'))]:run(name,image(marker+replacement+blob))
 for name,replacement in [('missingBundle',b''),('duplicateBundle',blob+blob),('corruptBundle',blob[:-1]+bytes([blob[-1]^1]))]:run(name,image(marker+boot+replacement))
 for name,index,value in [('chip',12,0),('modeHeader',2,0),('segmentsZero',1,0),('segmentsMany',1,17),('hashFlag',23,0)]:
  changed=bytearray(good);changed[index]=value;run(name,bytes(changed))
 for name,firmware in [('truncated',good[:-1]),('trailing',good+b'x'),('empty',b''),('oversized',b'x'*(0x330000+1)),('wrongChecksum',good[:-33]+bytes([good[-33]^1])+good[-32:]),('wrongImageHash',good[:-1]+bytes([good[-1]^1]))]:run(name,firmware)
 badDesc=bytearray(desc);badDesc[0]=0;run('noAppDescription',image(marker+boot+blob,badDesc))
 # All altered web manifests/images are consistently rehashed, so each case
 # reaches structural validation rather than merely failing an outer checksum.
 def web_case(name,entries):
  entries=sorted(entries,key=lambda pair:pair[0])
  manifest=('ESPSOMFY_ASSETS_V1\n'+''.join(hashlib.sha256(body).hexdigest()+' '+str(len(body))+' /'+name+'\n' for name,body in entries)).encode()
  compatible='sha256:'+hashlib.sha256(manifest).hexdigest()
  archive=MAGIC+struct.pack('>I',len(manifest))+manifest+b''.join(body for _,body in entries)
  ownMarker=marker.replace(compat.encode(),compatible.encode())
  firmware=image(ownMarker+boot+archive);path=p/'firmware.bin';path.write_bytes(firmware)
  subprocess.run([str(binary),name,str(path),hashlib.sha256(firmware).hexdigest(),compatible,'no'],check=True)
 entries=[(path.name,path.read_bytes()) for path in data.iterdir() if path.name!='update-assets.txt']
 for label,replacement in [('shortGzip',b'x'),('reservedGzipFlags',b'\x1f\x8b\x08\xe0'+b'\0'*16),('expandedGzip',b'\x1f\x8b\x08\0'+b'\0'*12+struct.pack('<I',8*1024*1024+1))]:
  web_case(label,[(name,replacement if name=='index.js.gz' else body) for name,body in entries])
 web_case('appversionOversize',[(name,b' '*20000+b'4.0.1' if name=='appversion' else body) for name,body in entries])
 web_case('privatePath',[(('shades.cfg' if name=='index.js.gz' else name),body) for name,body in entries])
 web_case('nestedPath',[(('sub/index.js.gz' if name=='index.js.gz' else name),body) for name,body in entries])
 web_case('longPath',[(('x'*128+'.js.gz' if name=='index.js.gz' else name),body) for name,body in entries])
 web_case('missingRequired',[(name,body) for name,body in entries if name!='app.css.gz'])
 web_case('duplicatePath',entries+[entries[0]])
 web_case('publicShadow',entries+[('index.js',b'hello')])
 web_case('tooManyAssets',entries+[(f'x{i:03d}.js',b'x') for i in range(125)])
 if os.environ.get('UPDATE_IMAGE_FIRMWARE'):
  from bundled_web_assets import extract
  firmware=Path(os.environ['UPDATE_IMAGE_FIRMWARE']);raw=firmware.read_bytes();web=extract(raw)
  markers=re.findall(rb'ESPSOMFY_BOOT_ID:[0-9]+:[0-9]+:[0-9a-f]{64}\0',raw);assert len(markers)==1
  subprocess.run([str(binary),'actual',str(firmware),hashlib.sha256(raw).hexdigest(),web['compatibility'],'yes',markers[0][:-1].decode()],check=True)
 print('Bounded real staged-image verification passed (local/fake, no flash/network)')

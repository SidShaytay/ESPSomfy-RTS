"""Validate build-only packages against one exact registered OTA profile."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import struct

from lilygo_manifest import PLATFORM, digest, source_record


def expected_defines(profile):
    from update_profiles import profile_defines
    result={name: ('"'+value+'"' if isinstance(value,str) else str(value)) if value is not None else '' for name,value in profile_defines(profile)}
    result.update({'ESPSOMFY_GITHUB_UPDATES':'','BOARD_HAS_PSRAM':'','IDF_VER':'"v5.5.5"'})
    return result


def image(data,name,profile):
    from update_profiles import validate_image
    try:
        validate_image(data,profile)
    except ValueError as error:
        raise ValueError(name+': '+str(error)) from error


def partitions(data,profile):
    if len(data)!=0xc00: raise ValueError('partition table must be 3072 bytes')
    found={};checked=False
    for offset in range(0,len(data),32):
        row=data[offset:offset+32]
        if row==b'\xff'*32:
            if data[offset:]!=b'\xff'*(len(data)-offset): raise ValueError('partition table non-padding suffix')
            break
        if row[:2]==b'\xeb\xeb':
            if checked or row[2:16]!=b'\xff'*14 or row[16:]!=hashlib.md5(data[:offset]).digest(): raise ValueError('partition MD5 is invalid')
            checked=True;continue
        if checked: raise ValueError('partition after MD5')
        magic,kind,sub,address,size,label,flags=struct.unpack('<HBBII16sI',row)
        label=label.rstrip(b'\0').decode('ascii')
        if magic!=0x50aa or flags or not label or label in found or not size or address<0x9000 or address+size>profile['flash_bytes']:
            raise ValueError('invalid partition entry')
        found[label]=[kind,sub,address,size]
    expected={'nvs':[1,2,profile['nvs_address'],profile['nvs_bytes']],
              'otadata':[1,0,0xe000,0x2000],
              'app0':[0,0x10,profile['app0_address'],profile['app_bytes']],
              'app1':[0,0x11,profile['app1_address'],profile['app_bytes']],
              'spiffs':[1,0x82,profile['fs_address'],profile['fs_bytes']]}
    if not checked or any(found.get(name)!=value for name,value in expected.items()): raise ValueError('partition layout differs from exact profile')
    regions=sorted((record[2],record[2]+record[3]) for record in found.values())
    if any(right[0]<left[1] for left,right in zip(regions,regions[1:])): raise ValueError('partition regions overlap')
    # Additional application slots could alter the bootloader's OTA selection.
    if any(record[0]==0 and name not in ('app0','app1') for name,record in found.items()): raise ValueError('unexpected application partition')
    return found


def configuration(directory,profile):
    environment=profile['board']
    resolved=json.loads((directory/'resolved-config.json').read_text())
    rows=resolved.items() if isinstance(resolved,dict) else resolved
    selected=[settings for name,settings in rows if name=='env:'+environment]
    if len(selected)!=1: raise ValueError('missing or duplicate resolved profile configuration')
    settings=dict(selected[0])
    if settings.get('platform')!=PLATFORM or settings.get('framework')!=['arduino'] or settings.get('build_unflags'):
        raise ValueError('build does not use the pinned Arduino platform')
    if any(settings.get(key)!=value for key,value in profile['build_settings'].items()): raise ValueError('resolved board settings differ from exact profile')
    compiler=json.loads((directory/'compiler-metadata.json').read_text())[environment]
    if compiler.get('env_name')!=environment or compiler.get('build_type')!='release': raise ValueError('compiler environment differs from release profile')
    required=expected_defines(profile)
    def normalize(value): return value.replace('\\"','"')
    for key,value in required.items():
        matches=[normalize(entry.partition('=')[2]) for entry in compiler.get('defines',[]) if entry.partition('=')[0]==key]
        if matches!=[normalize(value)]: raise ValueError('effective profile macro missing or conflicting: '+key)
    for field in ('cc_flags','cxx_flags'):
        tokens=shlex.split(' '.join(compiler.get(field,[])))
        for index,flag in enumerate(tokens):
            if flag in ('-D','-U') and index+1<len(tokens): flag+=tokens[index+1]
            if flag.startswith(('-D','-U')):
                name,_,value=flag[2:].partition('=')
                if name in required and (flag.startswith('-U') or normalize(value)!=normalize(required[name])): raise ValueError('compiler flag overrides profile: '+name)
    flash_images=compiler.get('extra',{}).get('flash_images',[])
    for filename,address in (('bootloader.bin',profile['bootloader_address']),('partitions.bin',0x8000)):
        entries=[entry for entry in flash_images if Path(entry.get('path','')).name==filename]
        try:
            valid=len(entries)==1 and int(entries[0]['offset'],0)==address and Path(entries[0]['path']).parent.name==environment
        except (KeyError,TypeError,ValueError):
            valid=False
        if not valid: raise ValueError('build flash image offset or environment differs: '+filename)
    suffix='/'+profile['chip']+'/'+profile['sdk_memory']+'/include'
    if not any(path.replace('\\','/').endswith(suffix) for path in compiler.get('includes',{}).get('build',[])): raise ValueError('compiler SDK memory configuration differs from exact profile')
    return {'resolved':settings,'compiler':{key:compiler.get(key) for key in ('env_name','build_type','defines','cc_flags','cxx_flags','includes','extra')}}


def artifact_manifest(directory,profile):
    directory=Path(directory)
    required={'firmware.bin','bootloader.bin','partitions.bin','littlefs.bin','firmware.elf','resolved-config.json','compiler-metadata.json','update-source.json'}
    allowed=required|{'boot_app0.bin','update-build.json','flash-manifest.json'}
    entries=list(directory.iterdir())
    if any(not path.is_file() or path.is_symlink() or path.name not in allowed for path in entries): raise ValueError('unexpected or unsafe package entry')
    if not required.issubset({path.name for path in entries}): raise ValueError('package lacks build evidence')
    if profile['psram_bytes']<0x800000: raise ValueError('profile lacks supported PSRAM capacity')
    files={}
    for path in entries:
        if path.name in ('update-build.json','flash-manifest.json'): continue
        data=path.read_bytes()
        if not data: raise ValueError('empty package file: '+path.name)
        if path.name in ('bootloader.bin','firmware.bin'): image(data,path.name,profile)
        if path.name=='bootloader.bin' and len(data)>0x8000-profile['bootloader_address']: raise ValueError('bootloader overlaps partition table')
        if path.name=='firmware.bin' and len(data)>profile['app_bytes']-8192: raise ValueError('firmware lacks 8KiB OTA slot headroom')
        if path.name=='littlefs.bin' and len(data)!=profile['fs_bytes']: raise ValueError('filesystem does not fill exact profile partition')
        files[path.name]={'size':len(data),'sha256':digest(data)}
    return {'schema':1,'profile':profile,'files':files,'partitions':partitions((directory/'partitions.bin').read_bytes(),profile),'effective_build':configuration(directory,profile)}


def validate(record,directory,profile):
    if record!=artifact_manifest(directory,profile): raise ValueError('recorded package evidence differs from actual profile artifacts')
    return record


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--source',required=True,type=Path)
    parser.add_argument('--environment',required=True)
    args=parser.parse_args()
    from update_profiles import load_profile
    try:
        profile=load_profile(args.source,args.environment)
        if not profile: raise ValueError('environment has no registered verified-update profile')
        compiled=json.loads((args.directory/'update-source.json').read_text())['source']
        current=source_record(args.source)
        if current!=compiled: raise ValueError('source changed since compilation; rebuild exact reviewed revision')
        record={'source':current,'artifacts':artifact_manifest(args.directory,profile)}
        (args.directory/'update-build.json').write_text(json.dumps(record,sort_keys=True,indent=2)+'\n')
    except (ValueError,KeyError,OSError) as error: parser.exit(1,f'Package rejected: {error}\n')
    print('Validated exact profile artifacts: '+profile['board'])

if __name__=='__main__': main()

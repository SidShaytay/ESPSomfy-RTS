"""Offline profile-specific build artifact rejection tests."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).parent))
import update_package as p
from update_profiles import load_profile, FEATURE_DEFINES


def profile(chip=9):
    return {'board':'fixture-s3' if chip==9 else 'fixture-p4','layout':'fixture_dual',
            'chip':'esp32s3' if chip==9 else 'esp32p4','chip_id':chip,
            'flash_bytes':0x1000000,'psram_bytes':0x800000,'app_bytes':0x330000,
            'app0_address':0x10000,'app1_address':0x340000,'fs_address':0x670000,
            'fs_bytes':0x180000,'nvs_address':0x9000,'nvs_bytes':0x5000,
            'bootloader_address':0 if chip==9 else 0x2000,'sdk_memory':'qio_opi' if chip==9 else 'qio_qspi',
            'required_defines':{'ESPSOMFY_GITHUB_UPDATES':''}, 'forbidden_defines':list(FEATURE_DEFINES),
            'build_settings':{'board':'fixture','build_type':'release','board_build.partitions':'fixture.csv'}}


def image(chip, flash_bytes=0x1000000, payload=None):
    header=bytearray(24);header[:4]=bytes([0xe9,1,2,({0x400000:2,0x800000:3,0x1000000:4,0x2000000:5}[flash_bytes]<<4)|15]);header[23]=1
    struct.pack_into('<H',header,12,chip)
    payload=payload if payload is not None else b'fixture image payload'*60
    data=header+struct.pack('<II',0x3c000000,len(payload))+payload
    checksum=0xef
    for byte in payload: checksum^=byte
    data+=b'\0'*((-len(data)-1)%16)+bytes([checksum])
    return bytes(data+hashlib.sha256(data).digest())


def table(spec):
    rows=[('nvs',1,2,spec['nvs_address'],spec['nvs_bytes']),('otadata',1,0,0xe000,0x2000),
          ('app0',0,0x10,spec['app0_address'],spec['app_bytes']),('app1',0,0x11,spec['app1_address'],spec['app_bytes']),
          ('spiffs',1,0x82,spec['fs_address'],spec['fs_bytes'])]
    raw=b''.join(struct.pack('<HBBII16sI',0x50aa,kind,sub,address,size,name.encode(),0) for name,kind,sub,address,size in rows)
    raw+=b'\xeb\xeb'+b'\xff'*14+hashlib.md5(raw).digest()
    return raw.ljust(0xc00,b'\xff')


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.directory=Path(self.temp.name);self.profile=profile()
        for name in ('firmware.bin','bootloader.bin'): (self.directory/name).write_bytes(image(9))
        (self.directory/'partitions.bin').write_bytes(table(self.profile))
        (self.directory/'littlefs.bin').write_bytes(b'\xff'*self.profile['fs_bytes'])
        (self.directory/'firmware.elf').write_bytes(b'ELF')
        self.record={'revision':'a'*40,'dirty':False,'input_sha256':{'platformio.ini':'b'*64}}
        (self.directory/'update-source.json').write_text(json.dumps({'source':self.record,'identity':{}}))
        self.write_metadata()

    def write_metadata(self):
        spec=self.profile
        settings=dict(spec['build_settings'],platform=p.PLATFORM,framework=['arduino'])
        settings['build_flags']=['-D'+name+('='+value if value else '') for name,value in spec.get('required_defines',{}).items()]
        defines=p.expected_defines(spec)
        compiler={'env_name':spec['board'],'build_type':'release','defines':[key+('='+value if value else '') for key,value in defines.items()],
                  'cc_flags':[],'cxx_flags':[],'includes':{'build':['/sdk/'+spec['chip']+'/'+spec['sdk_memory']+'/include']},
                  'extra':{'flash_images':[{'offset':hex(spec['bootloader_address']),'path':'/build/'+spec['board']+'/bootloader.bin'},{'offset':'0x8000','path':'/build/'+spec['board']+'/partitions.bin'}]}}
        (self.directory/'resolved-config.json').write_text(json.dumps([['env:'+spec['board'],list(settings.items())]]))
        (self.directory/'compiler-metadata.json').write_text(json.dumps({spec['board']:compiler}))

    def test_every_registered_profile_has_validated_exact_build_evidence(self):
        from update_profiles import supported_profiles
        for spec in supported_profiles(Path(__file__).resolve().parent.parent).values():
            with self.subTest(profile=spec['board']):
                self.profile=spec
                for name in ('firmware.bin','bootloader.bin'):
                    (self.directory/name).write_bytes(image(spec['chip_id'],spec['flash_bytes']))
                (self.directory/'partitions.bin').write_bytes(table(spec))
                (self.directory/'littlefs.bin').write_bytes(b'\xff'*spec['fs_bytes'])
                self.write_metadata()
                self.assertEqual(p.artifact_manifest(self.directory,spec)['profile']['board'],spec['board'])

    def variant(self, environment):
        self.profile=load_profile(Path(__file__).resolve().parent.parent,environment)
        self.write_metadata()
        return self.directory/'compiler-metadata.json',self.directory/'resolved-config.json'

    def test_compiled_preview_cannot_enable_physical_commands(self):
        compiler,_=self.variant('lilygo-t-embed-cc1101-plus-ui-preview')
        data=json.loads(compiler.read_text())
        data[self.profile['board']]['defines'].append('ESPSOMFY_LOCAL_COMMANDS=0')
        compiler.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,'forbidden.*ESPSOMFY_LOCAL_COMMANDS'):
            p.configuration(self.directory,self.profile)

    def test_compiled_headless_cannot_contain_display_features(self):
        for flag in ('ESPSOMFY_STATUS_DISPLAY','ESPSOMFY_LOCAL_UI','ESPSOMFY_LOCAL_COMMANDS'):
            with self.subTest(flag=flag):
                compiler,_=self.variant('lilygo-t-embed-cc1101-plus')
                data=json.loads(compiler.read_text())
                data[self.profile['board']]['cxx_flags']=['-D',flag]
                compiler.write_text(json.dumps(data))
                with self.assertRaisesRegex(ValueError,'forbidden'):
                    p.configuration(self.directory,self.profile)

    def test_display_profile_requires_its_actual_compiled_feature_and_board_macros(self):
        for flag in ('ESPSOMFY_STATUS_DISPLAY','ESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS'):
            with self.subTest(flag=flag):
                compiler,_=self.variant('lilygo-t-embed-cc1101-plus-display')
                data=json.loads(compiler.read_text())
                data[self.profile['board']]['defines']=[entry for entry in data[self.profile['board']]['defines'] if entry.partition('=')[0]!=flag]
                compiler.write_text(json.dumps(data))
                with self.assertRaisesRegex(ValueError,'macro'):
                    p.configuration(self.directory,self.profile)

    def test_resolved_flags_cannot_substitute_another_feature_variant(self):
        _,resolved=self.variant('lilygo-t-embed-cc1101-plus-ui-preview')
        data=json.loads(resolved.read_text())
        rows=dict(data[0][1]);rows['build_flags'].append('-DESPSOMFY_LOCAL_COMMANDS')
        data[0][1]=list(rows.items());resolved.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,'forbidden'):
            p.configuration(self.directory,self.profile)

    def test_resolved_ui_requires_command_feature_and_pinned_display_libraries(self):
        _,resolved=self.variant('lilygo-t-embed-cc1101-plus-ui')
        data=json.loads(resolved.read_text());rows=dict(data[0][1])
        rows['build_flags']=[flag for flag in rows['build_flags'] if flag!='-DESPSOMFY_LOCAL_COMMANDS']
        data[0][1]=list(rows.items());resolved.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,'feature'):
            p.configuration(self.directory,self.profile)
        _,resolved=self.variant('lilygo-t-embed-cc1101-plus-ui')
        data=json.loads(resolved.read_text());rows=dict(data[0][1]);rows['lib_deps'].append('adafruit/Adafruit GFX Library@99')
        data[0][1]=list(rows.items());resolved.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,'board settings'):
            p.configuration(self.directory,self.profile)

    def test_source_provenance_hashes_actual_partition_templates(self):
        import lilygo_manifest
        source=self.directory/'source';source.mkdir()
        (source/'platformio.ini').write_text('[env:fixture]')
        (source/'update-profiles.json').write_text('{"schema":1}')
        table=source/'esp32_psram_4MB.csv'
        table.write_text('app0,app,ota_0,0x10000,0x1b0000')
        with patch.object(lilygo_manifest,'git',side_effect=[None,'a'*40,' M already-dirty']):
            before=lilygo_manifest.source_record(source)
        self.assertIn('esp32_psram_4MB.csv',before['input_sha256'])
        self.assertIn('update-profiles.json',before['input_sha256'])
        table.write_text('app0,app,ota_0,0x10000,0x330000')
        with patch.object(lilygo_manifest,'git',side_effect=[None,'a'*40,' M already-dirty']):
            after=lilygo_manifest.source_record(source)
        self.assertEqual(before['revision'],after['revision'])
        self.assertEqual(before['dirty'],after['dirty'])
        self.assertNotEqual(before['input_sha256']['esp32_psram_4MB.csv'],after['input_sha256']['esp32_psram_4MB.csv'])

    def test_accepts_only_exact_profile_artifacts(self):
        evidence=p.artifact_manifest(self.directory,self.profile)
        self.assertEqual(evidence['profile'],self.profile)
        self.assertEqual(evidence['files']['firmware.bin']['sha256'],hashlib.sha256(image(9)).hexdigest())

    def test_accepts_p4_chip_and_rejects_s3_image_for_p4(self):
        self.profile=profile(18);self.write_metadata()
        with self.assertRaises(ValueError): p.artifact_manifest(self.directory,self.profile)
        for name in ('firmware.bin','bootloader.bin'): (self.directory/name).write_bytes(image(18))
        self.assertEqual(p.artifact_manifest(self.directory,self.profile)['profile']['chip_id'],18)

    def test_rejects_partition_layout_mismatch(self):
        changed=dict(self.profile,app1_address=0x350000)
        (self.directory/'partitions.bin').write_bytes(table(changed))
        with self.assertRaises(ValueError): p.artifact_manifest(self.directory,self.profile)

    def test_rejects_wrong_bootloader_flash_offset(self):
        path=self.directory/'compiler-metadata.json';metadata=json.loads(path.read_text())
        metadata[self.profile['board']]['extra']['flash_images'][0]['offset']='0x2000'
        path.write_text(json.dumps(metadata))
        with self.assertRaisesRegex(ValueError,'flash image offset'): p.artifact_manifest(self.directory,self.profile)

    def test_rejects_effective_macro_override(self):
        path=self.directory/'compiler-metadata.json';metadata=json.loads(path.read_text())
        metadata[self.profile['board']]['cxx_flags']=['-UESPSOMFY_GITHUB_UPDATES']
        path.write_text(json.dumps(metadata))
        with self.assertRaises(ValueError): p.artifact_manifest(self.directory,self.profile)

    def test_rejects_sdk_memory_and_board_mismatch(self):
        for field,value in [('board','another-board'),('build_type','debug')]:
            settings=json.loads((self.directory/'resolved-config.json').read_text())
            settings[0][1].append([field,value]);(self.directory/'resolved-config.json').write_text(json.dumps(settings))
            with self.assertRaises(ValueError): p.artifact_manifest(self.directory,self.profile)
            self.write_metadata()

    def test_rejects_corruption_or_symlinks(self):
        path=self.directory/'firmware.bin';data=bytearray(path.read_bytes());data[35]^=1;path.write_bytes(data)
        with self.assertRaises(ValueError): p.artifact_manifest(self.directory,self.profile)
        path.unlink();path.symlink_to('/etc/hostname')
        with self.assertRaises(ValueError): p.artifact_manifest(self.directory,self.profile)

    def test_recorded_evidence_must_match_actual_bytes(self):
        evidence=p.artifact_manifest(self.directory,self.profile)
        evidence['files']['firmware.bin']['sha256']='0'*64
        with self.assertRaises(ValueError): p.validate(evidence,self.directory,self.profile)

if __name__=='__main__': unittest.main()

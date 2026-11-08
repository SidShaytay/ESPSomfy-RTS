"""Offline release provenance and exact-board rejection tests."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parent))
import update_release as r
import update_identity as i
import test_lilygo_manifest as fixtures


def image(chip, payload):
    import struct
    header=bytearray(24)
    header[:4]=bytes([0xe9,1,2,0x4f])
    header[23]=1
    struct.pack_into('<H',header,12,chip)
    data=header+struct.pack('<II',0x3c000000,len(payload))+payload
    checksum=0xef
    for byte in payload: checksum ^= byte
    data.extend(b'\0'*((-len(data)-1)%16)+bytes([checksum]))
    return data+hashlib.sha256(data).digest()

class IdentityTests(unittest.TestCase):
    def test_clean_exact_stable_tag(self):
        record = {'revision': '1' * 40, 'dirty': False, 'input_sha256': {}}
        identity = i.identity(record, 'sha256:' + 'a' * 64, ['v4.0.3'])
        self.assertEqual(identity['version'], 'v4.0.3')
        self.assertEqual(identity['marker'], 'ESPSOMFY_UPDATE_ID:2|lilygo-t-embed-cc1101|default_8MB|' + '1' * 40 + '|v4.0.3|sha256:' + 'a' * 64)

    def test_dirty_tagged_build_is_development(self):
        result = i.identity({'revision': '1' * 40, 'dirty': True}, 'sha256:' + 'a' * 64, ['v4.0.3'])
        self.assertTrue(result['version'].startswith('dev-'))
        self.assertNotEqual(result['version'], 'v4.0.3')

    def test_untagged_or_ambiguous_tags_are_development(self):
        for tags in ([], ['v4.0.3-rc.1'], ['v4.0.3', 'v4.0.4'], ['v4294967296.1.0']):
            self.assertTrue(i.identity({'revision': '1' * 40, 'dirty': False}, 'sha256:' + 'a' * 64, tags)['version'].startswith('dev-'))

    def test_rejects_invalid_identity_component(self):
        for app in ('4.0.1|oops', '4.0.1\nextra', '', 'a' * 32):
            with self.assertRaises(ValueError):
                i.identity({'revision': '1' * 40, 'dirty': False}, app, ['v4.0.3'])


class ReleaseTests(unittest.TestCase):
    """Reuse independent valid ESP image/partition fixtures."""
    def setUp(self):
        fixtures.ManifestTests.setUp(self)
        self.profile = r.load_profile(Path(__file__).resolve().parent.parent, i.BOARD)
        self.profile_patch = patch.object(r, 'load_profile', return_value=self.profile)
        self.profile_patch.start()
        self.addCleanup(self.profile_patch.stop)
        compiler=json.loads((self.out/'compiler-metadata.json').read_text())
        definitions=compiler[i.BOARD]['defines']
        (self.out/'compiler-metadata.json').write_text(json.dumps(compiler))
        self.epoch_patch=patch.object(r,'source_epoch',return_value=1760000000)
        self.epoch_patch.start();self.addCleanup(self.epoch_patch.stop)
        self.record = dict(self.manifest['source'])
        self.identity = i.identity(self.record, 'sha256:' + 'a' * 64, ['v4.0.3'])
        for name,key in (('ESPSOMFY_SOURCE_SHA','source_sha'), ('ESPSOMFY_RELEASE_TAG','version'), ('ESPSOMFY_APP_COMPAT','app_compat')):
            definitions.append(name+'="'+self.identity[key]+'"')
        definitions.append('ESPSOMFY_SOURCE_EPOCH=1760000000')
        (self.out/'compiler-metadata.json').write_text(json.dumps(compiler))
        # Put the actual identity inside the checked image's data segment.
        firmware = self.out / 'firmware.bin'
        raw = firmware.read_bytes()
        bootloader = (self.out / 'bootloader.bin').read_bytes()
        boot_marker = f'ESPSOMFY_BOOT_ID:{self.profile["bootloader_address"]}:{len(bootloader)}:{hashlib.sha256(bootloader).hexdigest()}'.encode() + b'\0'
        payload = (self.identity['marker'].encode() + b'\0' + boot_marker).ljust(1024, b'\0')
        import struct
        image = bytearray(raw[:24]) + struct.pack('<II', 0x3c000000, len(payload)) + payload
        checksum = 0xef
        for byte in payload:
            checksum ^= byte
        image.extend(b'\0' * ((-len(image) - 1) % 16) + bytes([checksum]))
        image.extend(hashlib.sha256(image).digest())
        firmware.write_bytes(image)
        self.manifest = r.build.artifact_manifest(self.out)
        self.manifest['source'] = self.record
        (self.out / 'flash-manifest.json').write_text(json.dumps(self.manifest))
        (self.out / 'update-source.json').write_text(json.dumps({'source': self.record, 'identity': self.identity}))

    def package(self):
        with patch.object(r.build, 'source_record', return_value=self.record), patch.object(r, 'tags_at_head', return_value=['v4.0.3']), patch.object(r, 'read_app_compat', return_value='sha256:' + 'a' * 64):
            return r.release_manifest(self.out, Path('/source'), 'v4.0.3')

    def test_release_binds_actual_image_identity_and_digest(self):
        manifest = self.package()
        self.assertEqual(manifest['schema'],2)
        self.assertEqual(manifest['features'],7)
        self.assertEqual(manifest['compatible_boards'],[i.BOARD, i.BOARD+'-plus'])
        self.assertEqual(manifest['source_sha'], self.record['revision'])
        self.assertEqual(manifest['app_compat'], 'sha256:' + 'a' * 64)
        self.assertEqual(manifest['size'], (self.out / 'firmware.bin').stat().st_size)
        self.assertEqual(manifest['sha256'], hashlib.sha256((self.out / 'firmware.bin').read_bytes()).hexdigest())
        self.assertEqual(manifest['asset'], 'firmware-lilygo-t-embed-cc1101.bin')
        self.assertEqual(manifest['bootloader_sha256'], hashlib.sha256((self.out / 'bootloader.bin').read_bytes()).hexdigest())

    def test_rejects_changed_source_after_build(self):
        self.record = dict(self.record, input_sha256={'platformio.ini': '9' * 64})
        with self.assertRaisesRegex(ValueError, 'source changed'):
            self.package()

    def test_rejects_dirty_build_even_when_source_is_now_clean(self):
        record = dict(self.record, dirty=True)
        (self.out / 'update-source.json').write_text(json.dumps({'source': record, 'identity': self.identity}))
        with self.assertRaises(ValueError):
            self.package()

    def test_rejects_relabelled_version(self):
        with patch.object(r.build, 'source_record', return_value=self.record), patch.object(r, 'tags_at_head', return_value=['v4.0.4']), patch.object(r, 'read_app_compat', return_value='sha256:' + 'a' * 64):
            with self.assertRaises(ValueError):
                r.release_manifest(self.out, Path('/source'), 'v4.0.4')

    def test_rejects_wrong_application_compatibility(self):
        with patch.object(r.build, 'source_record', return_value=self.record), patch.object(r, 'tags_at_head', return_value=['v4.0.3']), patch.object(r, 'read_app_compat', return_value='sha256:' + 'b' * 64):
            with self.assertRaisesRegex(ValueError, 'identity mismatch'):
                r.release_manifest(self.out, Path('/source'), 'v4.0.3')

    def test_rejects_version_outside_runtime_integer_bounds(self):
        with self.assertRaisesRegex(ValueError, 'stable'):
            r.release_manifest(self.out, Path('/source'), 'v4294967296.1.0')

    def test_rejects_missing_or_conflicting_compiled_bootloader_marker(self):
        bootloader=(self.out/'bootloader.bin').read_bytes()
        boot_marker=f'ESPSOMFY_BOOT_ID:{self.profile["bootloader_address"]}:{len(bootloader)}:{hashlib.sha256(bootloader).hexdigest()}'.encode()+b'\0'
        for suffix in (b'',boot_marker+boot_marker,boot_marker+b'ESPSOMFY_BOOT_ID:malformed\0'):
            with self.subTest(suffix=suffix):
                payload=(self.identity['marker'].encode()+b'\0'+suffix).ljust(1024,b'\0')
                (self.out/'firmware.bin').write_bytes(image(9,payload=payload))
                record=r.build.artifact_manifest(self.out)
                record['source']=self.record
                (self.out/'flash-manifest.json').write_text(json.dumps(record))
                with self.assertRaisesRegex(ValueError,'compiled bootloader identity'):
                    self.package()

    def test_rejects_different_valid_bootloader_even_when_package_is_rehashed(self):
        (self.out / 'bootloader.bin').write_bytes(image(9,payload=b'different valid bootloader'))
        record=r.build.artifact_manifest(self.out)
        record['source']=self.record
        (self.out / 'flash-manifest.json').write_text(json.dumps(record))
        with self.assertRaisesRegex(ValueError,'compiled bootloader identity'):
            self.package()

    def test_rejects_missing_prebuild_identity(self):
        (self.out / 'update-source.json').unlink()
        with self.assertRaises((ValueError, FileNotFoundError)):
            self.package()

    def test_rejects_another_board_identity(self):
        self.identity['marker'] = self.identity['marker'].replace('cc1101', 'cc1101-other')
        (self.out / 'update-source.json').write_text(json.dumps({'source': self.record, 'identity': self.identity}))
        with self.assertRaises(ValueError):
            self.package()

    def test_compiler_identity_cannot_be_removed_or_overridden(self):
        path=self.out/'compiler-metadata.json'
        original=json.loads(path.read_text())
        import copy
        for field in ('defines','cc_flags','cxx_flags'):
            metadata=copy.deepcopy(original)
            if field == 'defines':
                metadata[i.BOARD][field]=[v for v in metadata[i.BOARD][field] if not v.startswith('ESPSOMFY_SOURCE_SHA=')]
            else:
                metadata[i.BOARD][field]=['-U','ESPSOMFY_SOURCE_SHA']
            path.write_text(json.dumps(metadata))
            record=r.build.artifact_manifest(self.out);record['source']=self.record
            (self.out/'flash-manifest.json').write_text(json.dumps(record))
            with self.subTest(field=field), self.assertRaises(ValueError): self.package()
        path.write_text(json.dumps(original))

    def test_untagged_source_and_retired_environment_never_release(self):
        with patch.object(r.build, 'source_record', return_value=self.record), patch.object(r, 'tags_at_head', return_value=[]), patch.object(r, 'read_app_compat', return_value='sha256:'+'a'*64):
            with self.assertRaises(ValueError): r.release_manifest(self.out,Path('/source'),'v4.0.3')
        self.profile_patch.stop()
        for environment in ('esp32s3', 'lilygo-t-embed-cc1101-plus', 'lilygo-t-embed-cc1101-ui'):
            with self.subTest(environment=environment), self.assertRaises(ValueError):
                r.release_manifest(self.out,Path(__file__).resolve().parent.parent,'v4.0.3',environment)

    def test_compiler_clock_floor_cannot_differ_from_source_commit(self):
        path=self.out/'compiler-metadata.json';metadata=json.loads(path.read_text())
        metadata[i.BOARD]['defines']=[value for value in metadata[i.BOARD]['defines'] if not value.startswith('ESPSOMFY_SOURCE_EPOCH=')]
        metadata[i.BOARD]['defines'].append('ESPSOMFY_SOURCE_EPOCH=0')
        path.write_text(json.dumps(metadata));record=r.build.artifact_manifest(self.out);record['source']=self.record
        (self.out/'flash-manifest.json').write_text(json.dumps(record))
        with self.assertRaises(ValueError): self.package()

    def test_publish_output_is_exclusive(self):
        with tempfile.TemporaryDirectory() as output:
            with self.assertRaises(FileExistsError):
                r.write_release(self.out, Path('/source'), 'v4.0.3', Path(output))


if __name__ == '__main__': unittest.main()

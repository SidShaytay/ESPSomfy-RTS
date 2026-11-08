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
from test_lilygo_manifest import ManifestTests


class IdentityTests(unittest.TestCase):
    def test_clean_exact_stable_tag(self):
        record = {'revision': '1' * 40, 'dirty': False, 'input_sha256': {}}
        identity = i.identity(record, 'sha256:' + 'a' * 64, ['v4.0.3'])
        self.assertEqual(identity['version'], 'v4.0.3')
        self.assertEqual(identity['marker'], 'ESPSOMFY_UPDATE_ID:1|lilygo-t-embed-cc1101-plus|default_8MB|' + '1' * 40 + '|v4.0.3|sha256:' + 'a' * 64)

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


class ReleaseTests(ManifestTests):
    """Reuse independent valid ESP image/partition fixtures."""
    def setUp(self):
        super().setUp()
        self.profile = r.load_profile(Path(__file__).resolve().parent.parent, i.BOARD)
        self.profile_patch = patch.object(r, 'load_profile', return_value=self.profile)
        self.profile_patch.start()
        self.addCleanup(self.profile_patch.stop)
        # Legacy Plus evidence still has to prove the exact headless features.
        resolved=json.loads((self.out/'resolved-config.json').read_text())
        settings=dict(resolved[0][1])
        settings['build_flags']=['-D'+name+('='+value if value else '') for name,value in self.profile['required_defines'].items()]
        resolved[0][1]=list(settings.items());(self.out/'resolved-config.json').write_text(json.dumps(resolved))
        compiler=json.loads((self.out/'compiler-metadata.json').read_text())
        definitions=compiler[i.BOARD]['defines']
        for name,value in self.profile['required_defines'].items():
            if not any(entry.partition('=')[0]==name for entry in definitions): definitions.append(name+('='+value if value else ''))
        (self.out/'compiler-metadata.json').write_text(json.dumps(compiler))
        self.record = dict(self.manifest['source'])
        self.identity = i.identity(self.record, 'sha256:' + 'a' * 64, ['v4.0.3'])
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
        self.manifest['build_identity'] = self.identity
        (self.out / 'flash-manifest.json').write_text(json.dumps(self.manifest))
        (self.out / 'update-source.json').write_text(json.dumps({'source': self.record, 'identity': self.identity}))

    def package(self):
        with patch.object(r.build, 'source_record', return_value=self.record), patch.object(r, 'tags_at_head', return_value=['v4.0.3']), patch.object(r, 'read_app_compat', return_value='sha256:' + 'a' * 64):
            return r.release_manifest(self.out, Path('/source'), 'v4.0.3')

    def test_release_binds_actual_image_identity_and_digest(self):
        manifest = self.package()
        self.assertEqual(manifest['source_sha'], self.record['revision'])
        self.assertEqual(manifest['app_compat'], 'sha256:' + 'a' * 64)
        self.assertEqual(manifest['size'], (self.out / 'firmware.bin').stat().st_size)
        self.assertEqual(manifest['sha256'], hashlib.sha256((self.out / 'firmware.bin').read_bytes()).hexdigest())
        self.assertEqual(manifest['asset'], 'firmware-lilygo-t-embed-cc1101-plus.bin')
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
        from test_update_package import image
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
        from test_update_package import image
        (self.out / 'bootloader.bin').write_bytes(image(9,payload=b'different valid bootloader'))
        record=r.build.artifact_manifest(self.out)
        record['source']=self.record
        (self.out / 'flash-manifest.json').write_text(json.dumps(record))
        with self.assertRaisesRegex(ValueError,'compiled bootloader identity'):
            self.package()

    def test_legacy_plus_evidence_cannot_bypass_headless_capability_policy(self):
        path=self.out/'compiler-metadata.json';metadata=json.loads(path.read_text())
        metadata[i.BOARD]['defines'].append('ESPSOMFY_LOCAL_COMMANDS')
        path.write_text(json.dumps(metadata))
        # Even recomputing legacy hashes/evidence must not bless wrong features.
        manifest=r.build.artifact_manifest(self.out);manifest['source']=self.record
        (self.out/'flash-manifest.json').write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError,'forbidden'):
            self.package()

    def test_rejects_missing_prebuild_identity(self):
        (self.out / 'update-source.json').unlink()
        with self.assertRaises((ValueError, FileNotFoundError)):
            self.package()

    def test_rejects_another_board_identity(self):
        self.identity['marker'] = self.identity['marker'].replace('cc1101-plus', 'cc1101-other')
        (self.out / 'update-source.json').write_text(json.dumps({'source': self.record, 'identity': self.identity}))
        with self.assertRaises(ValueError):
            self.package()

    def test_publish_output_is_exclusive(self):
        with tempfile.TemporaryDirectory() as output:
            with self.assertRaises(FileExistsError):
                r.write_release(self.out, Path('/source'), 'v4.0.3', Path(output))


class ProfiledReleaseTests(unittest.TestCase):
    def test_every_registered_profile_releases_only_its_exact_compiled_identity(self):
        from update_profiles import supported_profiles
        from test_update_package import PackageTests, image, table
        import update_package
        profiles=supported_profiles(Path(__file__).resolve().parent.parent)
        for environment, spec in profiles.items():
            with self.subTest(profile=environment), tempfile.TemporaryDirectory() as directory:
                package=Path(directory)
                source={'revision':'a'*40,'dirty':False,'input_sha256':{'platformio.ini':'b'*64}}
                compat='sha256:'+'c'*64
                identity=i.identity(source,compat,['v4.0.3'],spec)
                bootloader=image(spec['chip_id'],spec['flash_bytes'])
                boot_marker=f'ESPSOMFY_BOOT_ID:{spec["bootloader_address"]}:{len(bootloader)}:{hashlib.sha256(bootloader).hexdigest()}'.encode()+b'\0'
                payload=(identity['marker'].encode()+b'\0'+boot_marker).ljust(1024,b'\0')
                (package/'firmware.bin').write_bytes(image(spec['chip_id'],spec['flash_bytes'],payload))
                (package/'bootloader.bin').write_bytes(bootloader)
                (package/'partitions.bin').write_bytes(table(spec))
                (package/'littlefs.bin').write_bytes(b'\xff'*spec['fs_bytes'])
                (package/'firmware.elf').write_bytes(b'ELF')
                (package/'update-source.json').write_text(json.dumps({'source':source,'identity':identity}))
                fixture=PackageTests();fixture.profile=spec;fixture.directory=package;fixture.write_metadata()
                (package/'update-build.json').write_text(json.dumps({'source':source,'artifacts':update_package.artifact_manifest(package,spec)}))
                with patch.object(r,'load_profile',return_value=spec), patch.object(r.build,'source_record',return_value=source), patch.object(r,'tags_at_head',return_value=['v4.0.3']), patch.object(r,'read_app_compat',return_value=compat):
                    manifest=r.release_manifest(package,Path('/source'),'v4.0.3',environment)
                    self.assertEqual(manifest['board'],environment)
                    self.assertEqual(manifest['asset'],'firmware-'+environment+'.bin')
                    self.assertEqual(manifest['layout'],spec['layout'])
                    self.assertEqual(manifest['flash_bytes'],spec['flash_bytes'])
                    self.assertEqual(manifest['psram_bytes'],spec['psram_bytes'])
                    self.assertEqual(manifest['bootloader_sha256'],hashlib.sha256((package/'bootloader.bin').read_bytes()).hexdigest())
                    changed=dict(spec,board=environment+'-other')
                    with patch.object(r,'load_profile',return_value=changed), self.assertRaises(ValueError):
                        r.release_manifest(package,Path('/source'),'v4.0.3',environment)

    def test_unregistered_environment_never_falls_back_to_a_generic_binary(self):
        with patch.object(r,'load_profile',return_value=None), self.assertRaisesRegex(ValueError,'registered'):
            r.release_manifest(Path('/missing'),Path('/source'),'v4.0.3','esp32s3')


if __name__ == '__main__':
    unittest.main()

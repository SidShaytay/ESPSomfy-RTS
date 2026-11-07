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
        self.record = dict(self.manifest['source'])
        self.identity = i.identity(self.record, 'sha256:' + 'a' * 64, ['v4.0.3'])
        # Put the actual identity inside the checked image's data segment.
        firmware = self.out / 'firmware.bin'
        raw = firmware.read_bytes()
        payload = (self.identity['marker'].encode() + b'\0').ljust(1024, b'\0')
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


if __name__ == '__main__':
    unittest.main()

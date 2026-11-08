"""Exact shared profile rejection; local files only."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
import update_profiles as p

ROOT = Path(__file__).resolve().parent.parent

class ProfileTests(unittest.TestCase):
    def test_one_shared_profile_and_no_retired_or_generic_alias(self):
        self.assertEqual(list(p.supported_profiles(ROOT)), ['lilygo-t-embed-cc1101'])
        for env in ('native', 'esp32dev', 'esp32s3', 'esp32s3-psram', 'esp32p4-psram',
                    'lilygo-t-embed-cc1101-plus', 'lilygo-t-embed-cc1101-ui',
                    'lilygo-t-embed-cc1101-plus-ui', 'lilygo-t-embed-cc1101-ui-preview'):
            with self.subTest(env=env): self.assertIsNone(p.load_profile(ROOT, env))

    def test_every_hardware_layout_feature_value_is_exact(self):
        original = json.loads((ROOT / 'update-profiles.json').read_text())
        for key, value in original['profiles']['lilygo-t-embed-cc1101'].items():
            document = copy.deepcopy(original)
            document['profiles']['lilygo-t-embed-cc1101'][key] = (value + 1 if type(value) is int else 'generic')
            with self.subTest(key=key), tempfile.TemporaryDirectory() as directory:
                source = Path(directory)
                (source / 'update-profiles.json').write_text(json.dumps(document))
                with self.assertRaises(ValueError): p.supported_profiles(source)

    def test_unknown_profile_and_old_schema_rejected(self):
        original = json.loads((ROOT / 'update-profiles.json').read_text())
        for document in (dict(original, schema=1), dict(original, profiles=dict(original['profiles'], esp32s3={}))):
            with tempfile.TemporaryDirectory() as directory:
                source = Path(directory)
                (source / 'update-profiles.json').write_text(json.dumps(document))
                with self.assertRaises(ValueError): p.supported_profiles(source)

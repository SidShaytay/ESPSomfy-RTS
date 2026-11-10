"""Run the actual pre-build script against local fake PlatformIO environments."""
from pathlib import Path
import runpy
import tempfile
import unittest
from unittest.mock import patch
import update_identity
import update_assets
import lilygo_manifest

ROOT = Path(__file__).resolve().parent.parent

class BuildIdentityTests(unittest.TestCase):
    def run_script(self, environment, source):
        class Env:
            def __init__(self): self.defines = []
            def get(self, key): return environment if key == 'PIOENV' else str(source)
            def subst(self, value): return str(source / 'build')
            def Append(self, CPPDEFINES=(), LINKFLAGS=()):
                self.defines.extend(CPPDEFINES)
                if LINKFLAGS: assert LINKFLAGS == ["-Wl,--undefined=espsomfyUpdateIdentity"]
        env = Env()
        runpy.run_path(str(ROOT / 'app_version.py'), init_globals={'env':env, 'Import':lambda _:None})
        return dict(env.defines)

    def test_generic_retains_version_and_no_updater_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            (source / 'data-src').mkdir()
            (source / 'data-src/appversion').write_text('4.0.2\n')
            result = self.run_script('esp32dev', source)
            self.assertEqual(result, {'FW_VERSION': '\\\"v4.0.2\\\"'})
            self.assertFalse((source / 'build').exists())

    def test_shared_build_derives_dev_identity_and_requires_exact_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            (source / 'data').mkdir()
            (source / 'data/index.js.gz').write_bytes(b'js')
            compatibility = update_assets.write_manifest(source / 'data')
            (source / 'update-profiles.json').write_bytes((ROOT / 'update-profiles.json').read_bytes())
            record = {'revision':'a'*40, 'dirty':True, 'input_sha256':{}}
            with patch.object(lilygo_manifest, 'source_record', return_value=record), patch.object(update_identity, 'tags_at_head', return_value=['v4.0.3']), patch.object(update_identity,'git',return_value='1760000000'):
                result = self.run_script('lilygo-t-embed-cc1101', source)
                self.assertIn('dev-aaaaaaaaaaaa-dirty', result['ESPSOMFY_RELEASE_TAG'])
                self.assertIn(compatibility, result['ESPSOMFY_APP_COMPAT'])
                (source / 'data/index.js.gz').write_bytes(b'changed')
                with self.assertRaises(ValueError): self.run_script('lilygo-t-embed-cc1101', source)

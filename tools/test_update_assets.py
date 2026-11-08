"""Offline exact web-image compatibility tests; no controller connection."""
import gzip
import hashlib
import importlib.util
from pathlib import Path
import runpy
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parent))
import update_assets as assets


class AssetTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.data = Path(self.temp.name) / 'data'
        self.data.mkdir()
        (self.data / 'index.html.gz').write_bytes(b'packed HTML')
        (self.data / 'appversion').write_text('4.0.1\n')

    def test_content_derived_compatibility_is_stable_and_verifies_all_bytes(self):
        first = assets.write_manifest(self.data)
        self.assertEqual(first, assets.write_manifest(self.data))
        self.assertEqual(first, assets.verify_manifest(self.data))
        self.assertEqual(first, 'sha256:' + hashlib.sha256((self.data / assets.FILENAME).read_bytes()).hexdigest())
        (self.data / 'index.html.gz').write_bytes(b'packed HTML modified')
        with self.assertRaises(ValueError):
            assets.verify_manifest(self.data)
        self.assertNotEqual(first, assets.write_manifest(self.data))

    def test_same_version_with_changed_ui_has_different_compatibility(self):
        before = assets.write_manifest(self.data)
        (self.data / 'index.js.gz').write_bytes(b'new Github updater UI')
        self.assertNotEqual(before, assets.write_manifest(self.data))

    def test_manifest_is_canonical_sorted_and_excludes_itself(self):
        assets.write_manifest(self.data)
        expected = assets.HEADER + ''.join(
            f'{hashlib.sha256((self.data / name).read_bytes()).hexdigest()} {(self.data / name).stat().st_size} /{name}\n'
            for name in ['appversion', 'index.html.gz'])
        self.assertEqual((self.data / assets.FILENAME).read_text(), expected)

    def test_rejects_stale_missing_extra_or_tampered_packaged_assets(self):
        assets.write_manifest(self.data)
        (self.data / 'extra.css.gz').write_bytes(b'extra')
        with self.assertRaises(ValueError): assets.verify_manifest(self.data)
        (self.data / 'extra.css.gz').unlink()
        (self.data / 'appversion').unlink()
        with self.assertRaises(ValueError): assets.verify_manifest(self.data)

    def test_nested_paths_use_the_same_lexical_order_as_runtime(self):
        (self.data / 'folder').mkdir()
        (self.data / 'folder' / 'index.js.gz').write_bytes(b'javascript')
        (self.data / 'folder.css.gz').write_bytes(b'css')
        assets.write_manifest(self.data)
        names = [line.split(' ', 2)[2] for line in (self.data / assets.FILENAME).read_text().splitlines()[1:]]
        self.assertEqual(names, sorted(names))

    def test_refuses_symlink_or_unsafe_path(self):
        (self.data / 'link').symlink_to(self.data / 'appversion')
        with self.assertRaises(ValueError): assets.write_manifest(self.data)
        (self.data / 'link').unlink()
        (self.data / 'has space.css').write_bytes(b'x')
        with self.assertRaises(ValueError): assets.write_manifest(self.data)

    def test_bounded_manifest_capacity_and_missing_manifest(self):
        with self.assertRaises(FileNotFoundError): assets.verify_manifest(self.data)
        for limit in ('MAX_FILES', 'MAX_MANIFEST', 'MAX_FILE'):
            with self.subTest(limit=limit), patch.object(assets,limit,1):
                with self.assertRaises(ValueError): assets.write_manifest(self.data)

    def test_shared_packaging_is_reproducible_and_ignores_optional_minifiers(self):
        project = Path(self.temp.name) / 'project'
        root = Path(__file__).resolve().parent.parent
        shutil.copytree(root / 'data-src', project / 'data-src')
        shutil.copyfile(root / 'update-profiles.json', project / 'update-profiles.json')
        class Env:
            def get(self, key): return 'lilygo-t-embed-cc1101' if key == 'PIOENV' else str(project)
            def subst(self, value): return str(project)
        # Bundler executes the real deterministic gzip path twice.
        namespace = {'env': Env(), 'Import': lambda _: None}
        with patch('gzip.time.time', return_value=1700000000), patch('shutil.which', return_value='/fake/optional-tool'), patch('subprocess.run', side_effect=AssertionError('Plus packaging invoked optional tool')):
            runpy.run_path(str(root / 'minify.py'), init_globals=namespace)
        first = {p.relative_to(project / 'data').as_posix(): p.read_bytes() for p in (project / 'data').rglob('*') if p.is_file()}
        with patch('gzip.time.time', return_value=1800000000), patch('shutil.which', return_value='/fake/optional-tool'), patch('subprocess.run', side_effect=AssertionError('Plus packaging invoked optional tool')):
            runpy.run_path(str(root / 'minify.py'), init_globals=namespace)
        second = {p.relative_to(project / 'data').as_posix(): p.read_bytes() for p in (project / 'data').rglob('*') if p.is_file()}
        self.assertEqual(first, second)
        self.assertIn(assets.FILENAME, first)
        # The deterministic path preserves the tested existing concatenation,
        # including BOM removal/newlines; it invents no regex minification.
        for name, parts in {'index.js':['js/schema.js','js/model.js','js/client.js','js/views.js','js/pins.js','index.js'],
                            'app.css':['main.css','widgets.css','icons.css']}.items():
            expected=''.join((project/'data-src'/part).read_text(encoding='utf-8-sig')+'\n' for part in parts).encode()
            self.assertEqual(gzip.decompress(first[name+'.gz']),expected)
        self.assertEqual(assets.verify_manifest(project / 'data'), 'sha256:' + hashlib.sha256(first[assets.FILENAME]).hexdigest())

if __name__ == '__main__': unittest.main()

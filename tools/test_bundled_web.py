"""Offline firmware-bundled asset generation and privacy boundaries."""
import gzip
import hashlib
from pathlib import Path
import tempfile
import unittest
import update_assets
import bundled_web_assets

class BundleTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(); self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.data=self.root/'data';self.data.mkdir()
        (self.data/'index.html.gz').write_bytes(gzip.compress(b'<html>test</html>',mtime=0))
        (self.data/'index.js.gz').write_bytes(gzip.compress(b'window.test=1;',mtime=0))
        (self.data/'app.css.gz').write_bytes(gzip.compress(b'body{}',mtime=0))
        (self.data/'appversion').write_bytes(b'4.0.1\n')
        update_assets.write_manifest(self.data)
    def test_exact_deterministic_flash_table_and_source_version(self):
        first=bundled_web_assets.generate(self.data,self.root/'one.h')
        second=bundled_web_assets.generate(self.data,self.root/'two.h')
        self.assertEqual((self.root/'one.h').read_bytes(),(self.root/'two.h').read_bytes())
        self.assertEqual(first,second)
        self.assertEqual(first['version'],'4.0.1')
        self.assertEqual(first['compatibility'],update_assets.verify_manifest(self.data))
        self.assertEqual([a['path'] for a in first['assets']],['/app.css','/appversion','/index.html','/index.js'])
        for a in first['assets']:
            self.assertEqual(a['sha256'],hashlib.sha256((self.data/a['file']).read_bytes()).hexdigest())
        self.assertNotIn('/update-assets.txt',[a['path'] for a in first['assets']])
    def test_blob_inside_firmware_is_self_verifying_and_version_bound(self):
        result=bundled_web_assets.generate(self.data,self.root/'bundle.h')
        blob=bundled_web_assets.build_blob(self.data)
        extracted=bundled_web_assets.extract(b'firmware-prefix'+blob+b'firmware-tail')
        self.assertEqual(result,extracted)
        damaged=bytearray(blob);damaged[-1]^=1
        with self.assertRaises(ValueError):bundled_web_assets.extract(bytes(damaged))
        with self.assertRaises(ValueError):bundled_web_assets.extract(blob+blob)
        with self.assertRaises(ValueError):bundled_web_assets.extract(blob[:-1])
    def test_private_unknown_nested_or_shadow_files_never_enter_firmware(self):
        for name in ('shades.cfg','secrets.json','index.html','sub/index.js.gz','appversion.gz'):
            with self.subTest(name=name):
                extra=self.data/name;extra.parent.mkdir(exist_ok=True);extra.write_bytes(b'private')
                update_assets.write_manifest(self.data)
                with self.assertRaises(ValueError):bundled_web_assets.generate(self.data,self.root/'bad.h')
                extra.unlink()
        update_assets.write_manifest(self.data)
    def test_manifest_or_gzip_tampering_fails_closed(self):
        (self.data/'index.js.gz').write_bytes(b'corrupt')
        with self.assertRaises(ValueError):bundled_web_assets.generate(self.data,self.root/'bad.h')
        update_assets.write_manifest(self.data)
        with self.assertRaises(ValueError):bundled_web_assets.generate(self.data,self.root/'bad.h')
    def test_gzip_expansion_is_bounded(self):
        (self.data/'index.js.gz').write_bytes(gzip.compress(b'x'*(bundled_web_assets.MAX_EXPANDED+1),mtime=0))
        update_assets.write_manifest(self.data)
        with self.assertRaises(ValueError):bundled_web_assets.generate(self.data,self.root/'bad.h')
    def test_version_and_required_entry_missing_fail_closed(self):
        for bad in (b'',b'x'*15,b'4.0.1\0secret',b'4.0.1\rprivate'):
            (self.data/'appversion').write_bytes(bad);update_assets.write_manifest(self.data)
            with self.assertRaises(ValueError):bundled_web_assets.generate(self.data,self.root/'bad.h')
        (self.data/'appversion').write_bytes(b'4.0.1\n');(self.data/'index.html.gz').unlink();update_assets.write_manifest(self.data)
        with self.assertRaises(ValueError):bundled_web_assets.generate(self.data,self.root/'bad.h')
if __name__=='__main__':unittest.main()

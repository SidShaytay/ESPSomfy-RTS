"""Offline artifact rejection tests; no device or installed toolchain required."""
import hashlib
import json
import os
import subprocess
import sys
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('manifest', Path(__file__).with_name('lilygo_manifest.py'))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


# Arduino 3.3.12 tools/partitions/default_8MB.csv, independent of the validator.
STANDARD_PARTITIONS = {
    'nvs': (1, 2, 0x9000, 0x5000),
    'otadata': (1, 0, 0xe000, 0x2000),
    'app0': (0, 0x10, 0x10000, 0x330000),
    'app1': (0, 0x11, 0x340000, 0x330000),
    'spiffs': (1, 0x82, 0x670000, 0x180000),
    'coredump': (1, 3, 0x7f0000, 0x10000),
}


class ManifestTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.out = Path(self.tmp.name)
        table = b''.join(struct.pack('<HBBII16sI', 0x50aa, kind, sub, offset, size,
                                     name.encode(), 0)
                         for name, (kind, sub, offset, size) in STANDARD_PARTITIONS.items())
        table += b'\xeb\xeb' + b'\xff' * 14 + hashlib.md5(table).digest()
        (self.out / 'partitions.bin').write_bytes(table.ljust(0xc00, b'\xff'))
        for name in ('bootloader.bin', 'firmware.bin'):
            header = bytearray(24)
            header[0:4] = bytes([0xe9, 1, 2, 0x4f])
            header[23] = 1
            struct.pack_into('<H', header, 12, 9)
            payload = b'payload'
            image = header + struct.pack('<II', 0x3c000000, len(payload)) + payload
            checksum = 0xef
            for byte in payload:
                checksum ^= byte
            image.extend(b'\0' * ((-len(image) - 1) % 16) + bytes([checksum]))
            image.extend(hashlib.sha256(image).digest())
            (self.out / name).write_bytes(image)
        ota = bytearray(b'\xff' * 0x2000)
        ota[0:4] = b'\x01\x00\x00\x00'
        ota[28:32] = bytes.fromhex('9a984347')
        ota[4096:4100] = b'\x00' * 4
        (self.out / 'boot_app0.bin').write_bytes(ota)
        (self.out / 'littlefs.bin').write_bytes(b'\xff' * 0x180000)
        (self.out / 'firmware.elf').write_bytes(b'ELF-symbols')
        profile = dict(m.BUILD_SETTINGS, lib_deps=list(m.DISPLAY_LIBRARIES), platform=m.PLATFORM, framework=['arduino'], build_flags=list(m.REQUIRED_FLAGS))
        compiler = {'env_name': m.ENV, 'build_type': 'release',
                    'defines': [f[2:] for f in m.REQUIRED_FLAGS] + ['BOARD_HAS_PSRAM', 'IDF_VER="v5.5.5"'],
                    'cc_flags': [], 'cxx_flags': [],
                    'includes': {'build': ['/sdk/esp32s3/qio_opi/include']}}
        (self.out / 'resolved-config.json').write_text(json.dumps([['env:' + m.ENV, list(profile.items())]]))
        (self.out / 'compiler-metadata.json').write_text(json.dumps({m.ENV: compiler}))
        self.manifest = m.artifact_manifest(self.out)
        self.manifest['source'] = {'revision': m.BASELINE['revision'], 'dirty': False,
                                   'input_sha256': {'platformio.ini': '0' * 64}}

    def test_canonical_package_includes_full_interface(self):
        self.assertFalse(self.manifest['target']['headless'])
        for feature in ('ESPSOMFY_STATUS_DISPLAY', 'ESPSOMFY_LOCAL_UI', 'ESPSOMFY_LOCAL_COMMANDS'):
            self.assertIn('-D' + feature, self.manifest['profile_build_flags'])

    def test_required_interface_cannot_be_removed_or_conflicted(self):
        import copy
        original = self.manifest['effective_build']
        for feature in ('ESPSOMFY_STATUS_DISPLAY', 'ESPSOMFY_LOCAL_UI', 'ESPSOMFY_LOCAL_COMMANDS'):
            for field in ('defines', 'cc_flags', 'cxx_flags'):
                evidence = copy.deepcopy(original)
                if field == 'defines':
                    evidence['compiler'][field] = [x for x in evidence['compiler'][field] if x != feature]
                else:
                    evidence['compiler'][field].append('-U' + feature)
                with self.subTest(feature=feature, field=field), self.assertRaises(ValueError):
                    m.build_configuration(evidence['resolved'], evidence['compiler'])
            for flags in (['-U', feature], ['-D' + feature + '=0']):
                evidence = copy.deepcopy(original)
                evidence['resolved']['build_flags'].extend(flags)
                with self.subTest(feature=feature, flags=flags), self.assertRaises(ValueError):
                    m.build_configuration(evidence['resolved'], evidence['compiler'])

    def test_opposite_physical_board_cannot_be_added(self):
        import copy
        original = self.manifest['effective_build']
        for field in ('defines', 'cc_flags', 'cxx_flags'):
            evidence = copy.deepcopy(original)
            evidence['compiler'][field].append(('' if field == 'defines' else '-D') + 'ESPSOMFY_LILYGO_T_EMBED_CC1101')
            with self.subTest(field=field), self.assertRaises(ValueError):
                m.build_configuration(evidence['resolved'], evidence['compiler'])

    def test_retired_profile_names_are_rejected(self):
        for suffix in ('-display', '-ui', '-ui-preview'):
            compiler = self.manifest['effective_build']['compiler'].copy()
            compiler['env_name'] = m.ENV + suffix
            with self.subTest(suffix=suffix), self.assertRaises(ValueError):
                m.build_configuration(self.manifest['effective_build']['resolved'], compiler)

    def test_standard_board_retains_distinct_identity(self):
        environment = 'lilygo-t-embed-cc1101'
        evidence = self.manifest['effective_build']
        profile = evidence['resolved'].copy()
        profile['build_flags'] = m.required_flags(environment)
        compiler = evidence['compiler'].copy()
        compiler['env_name'] = environment
        compiler['defines'] = [entry.replace('ESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS', 'ESPSOMFY_LILYGO_T_EMBED_CC1101')
                               for entry in compiler['defines']]
        (self.out / 'resolved-config.json').write_text(json.dumps([['env:' + environment, list(profile.items())]]))
        (self.out / 'compiler-metadata.json').write_text(json.dumps({environment: compiler}))
        manifest = m.artifact_manifest(self.out)
        self.assertEqual(manifest['target']['environment'], environment)
        self.assertEqual(manifest['target']['board'], 'LILYGO T-Embed CC1101')
        self.assertEqual(manifest['target']['profile_macro'], 'ESPSOMFY_LILYGO_T_EMBED_CC1101')
        self.assertFalse(manifest['target']['headless'])
        with self.assertRaises(ValueError):
            m.validate(self.manifest, self.out)

    def test_missing_pinned_display_dependency_is_rejected(self):
        evidence = self.manifest['effective_build']
        evidence['resolved']['lib_deps'] = m.DISPLAY_LIBRARIES[:-1]
        with self.assertRaises(ValueError):
            m.build_configuration(evidence['resolved'], evidence['compiler'])

    def test_only_two_full_feature_lilygo_environments(self):
        import configparser
        config = configparser.ConfigParser(interpolation=None)
        config.read(Path(__file__).resolve().parent.parent / 'platformio.ini')
        lilygo = {name for name in config.sections() if name.startswith('env:lilygo-')}
        self.assertEqual(lilygo, {'env:' + name for name in m.ENVIRONMENTS})
        for name in m.ENVIRONMENTS:
            flags = config['env:' + name]['build_flags'].split()
            self.assertIn('-D' + m.ENVIRONMENTS[name][1], flags)
            for feature in ('ESPSOMFY_STATUS_DISPLAY', 'ESPSOMFY_LOCAL_UI', 'ESPSOMFY_LOCAL_COMMANDS'):
                self.assertIn('-D' + feature, flags)
        for name in config.sections():
            if name.startswith('env:') and not name.startswith('env:lilygo-'):
                self.assertNotIn('ESPSOMFY_LOCAL_UI', config[name].get('build_flags', ''))
                self.assertNotIn('Adafruit', config[name].get('lib_deps', ''))

    def test_rejects_previous_custom_plus_layout(self):
        old = dict(STANDARD_PARTITIONS,
                   app0=(0, 0x10, 0x10000, 0x300000),
                   app1=(0, 0x11, 0x310000, 0x300000),
                   spiffs=(1, 0x82, 0x610000, 0x160000),
                   coredump=(1, 3, 0x770000, 0x10000))
        table = b''.join(struct.pack('<HBBII16sI', 0x50aa, kind, sub, offset, size,
                                     name.encode(), 0)
                         for name, (kind, sub, offset, size) in old.items())
        table += b'\xeb\xeb' + b'\xff' * 14 + hashlib.md5(table).digest()
        with self.assertRaisesRegex(ValueError, 'partition layout'):
            m.partition_table(table.ljust(0xc00, b'\xff'))

    def test_valid_package(self):
        m.validate(self.manifest, self.out)

    def test_stale_unlisted_image(self):
        (self.out / 'SomfyController.ino.bin').write_bytes(b'old build')
        with self.assertRaises(ValueError):
            m.artifact_manifest(self.out)

    def test_external_symlink(self):
        (self.out / 'firmware.elf').unlink()
        (self.out / 'firmware.elf').symlink_to('/etc/hostname')
        with self.assertRaises(ValueError):
            m.artifact_manifest(self.out)

    def test_tampered_image(self):
        (self.out / 'firmware.bin').write_bytes(b'bad')
        with self.assertRaises(ValueError):
            m.validate(self.manifest, self.out)

    def test_image_checksum_corruption_even_with_rehashed_manifest(self):
        data = bytearray((self.out / 'firmware.bin').read_bytes())
        data[32] ^= 1
        (self.out / 'firmware.bin').write_bytes(data)
        with self.assertRaises(ValueError):
            m.artifact_manifest(self.out)

    def test_image_hash_corruption_even_with_rehashed_manifest(self):
        data = bytearray((self.out / 'firmware.bin').read_bytes())
        data[-1] ^= 1
        (self.out / 'firmware.bin').write_bytes(data)
        with self.assertRaises(ValueError):
            m.artifact_manifest(self.out)

    def test_missing_source_provenance(self):
        del self.manifest['source']
        with self.assertRaises(ValueError):
            m.validate(self.manifest, self.out)

    def test_mismatched_memory_setting(self):
        self.manifest['build_settings']['board_build.psram_type'] = 'qspi'
        with self.assertRaises(ValueError):
            m.validate(self.manifest, self.out)

    def test_effective_conflicting_macro(self):
        evidence = self.manifest['effective_build']
        evidence['compiler']['defines'].append('ARDUINO_USB_MODE=0')
        with self.assertRaises(ValueError):
            m.build_configuration(evidence['resolved'], evidence['compiler'])

    def test_effective_removed_profile(self):
        evidence = self.manifest['effective_build']
        evidence['compiler']['cxx_flags'].append('-UESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS')
        with self.assertRaises(ValueError):
            m.build_configuration(evidence['resolved'], evidence['compiler'])

    def test_effective_unflags(self):
        evidence = self.manifest['effective_build']
        evidence['resolved']['build_unflags'] = ['-DARDUINO_USB_MODE=1']
        with self.assertRaises(ValueError):
            m.build_configuration(evidence['resolved'], evidence['compiler'])

    def test_effective_wrong_sdk_memory(self):
        evidence = self.manifest['effective_build']
        evidence['compiler']['includes'] = {'build': ['/sdk/esp32s3/dio_qspi/include']}
        with self.assertRaises(ValueError):
            m.build_configuration(evidence['resolved'], evidence['compiler'])

    def test_effective_wrong_source_memory(self):
        evidence = self.manifest['effective_build']
        evidence['resolved']['board_build.psram_type'] = 'qspi'
        with self.assertRaises(ValueError):
            m.build_configuration(evidence['resolved'], evidence['compiler'])

    def test_wrong_board(self):
        self.manifest['target']['board'] = 'T-Embed CC1101 (non-Plus)'
        with self.assertRaises(ValueError):
            m.validate(self.manifest, self.out)

    def test_changed_offset_even_with_valid_hash(self):
        self.manifest['segments'][3]['offset'] += 0x1000
        with self.assertRaises(ValueError):
            m.validate(self.manifest, self.out)

    def test_oversized_firmware_even_with_valid_hash(self):
        (self.out / 'firmware.bin').write_bytes(b'\x00' * 0x330001)
        with self.assertRaisesRegex(ValueError, 'image exceeds reserved region'):
            m.artifact_manifest(self.out)

    def test_wrong_flash_header_even_with_valid_hash(self):
        data = bytearray((self.out / 'bootloader.bin').read_bytes())
        data[3] = 0x2f  # 4MB instead of 16MB
        (self.out / 'bootloader.bin').write_bytes(data)
        with self.assertRaises(ValueError):
            m.artifact_manifest(self.out)

    def test_wrong_chip_header_even_with_valid_hash(self):
        data = bytearray((self.out / 'firmware.bin').read_bytes())
        data[12] = 0
        (self.out / 'firmware.bin').write_bytes(data)
        with self.assertRaises(ValueError):
            m.artifact_manifest(self.out)

    def test_wrong_partition_table_even_with_valid_hash(self):
        data = bytearray((self.out / 'partitions.bin').read_bytes())
        data[4] ^= 1
        (self.out / 'partitions.bin').write_bytes(data)
        with self.assertRaises(ValueError):
            m.artifact_manifest(self.out)

    def test_path_traversal(self):
        self.manifest['segments'][0]['file'] = '../bootloader.bin'
        with self.assertRaises(ValueError):
            m.validate(self.manifest, self.out)

    def test_wrong_baseline(self):
        self.manifest['baseline']['revision'] = '0' * 40
        with self.assertRaises(ValueError):
            m.validate(self.manifest, self.out)

    def test_provisioning_not_mislabelled_as_firmware_update(self):
        self.manifest['usage']['firmware_update'] = ['partitions.bin']
        with self.assertRaises(ValueError):
            m.validate(self.manifest, self.out)


class BuildScriptTests(unittest.TestCase):
    def test_retired_environment_is_refused_before_touching_package(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'artifacts').mkdir()
            marker = root / 'artifacts' / 'caller-owned'
            marker.write_text('preserve')
            repo = Path(__file__).resolve().parent.parent
            result = subprocess.run(['bash', str(repo / 'tools/build-lilygo.sh'), str(repo), str(root),
                                     'lilygo-t-embed-cc1101-plus-ui'], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Unsupported or retired', result.stderr)
            self.assertEqual(marker.read_text(), 'preserve')

    def test_failed_build_preserves_old_files_without_current_package(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cache = root / 'cache'
            tools = cache / 'pio-python' / 'bin'
            tools.mkdir(parents=True)
            (tools / 'python3').symlink_to(sys.executable)
            pio = tools / 'pio'
            pio.write_text('#!/bin/sh\nexit 23\n')
            pio.chmod(0o755)
            out = root / 'output'
            (out / 'artifacts').mkdir(parents=True)
            (out / 'artifacts' / 'old-arduino.bin').write_bytes(b'caller-owned')
            repo = Path(__file__).resolve().parent.parent
            result = subprocess.run(['bash', str(repo / 'tools/build-lilygo.sh'), str(repo), str(out)],
                                    env=dict(os.environ, ESPSOMFY_TOOL_CACHE=str(cache)),
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 23, result.stderr)
            self.assertFalse((out / 'artifacts').exists())
            preserved = list(out.glob('artifacts.previous.*/artifacts/old-arduino.bin'))
            self.assertEqual(len(preserved), 1)
            self.assertEqual(preserved[0].read_bytes(), b'caller-owned')
            self.assertEqual(list(out.glob('.artifacts.*')), [])


if __name__ == '__main__':
    unittest.main()

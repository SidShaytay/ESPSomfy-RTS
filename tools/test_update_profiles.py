"""Host-only exact-board updater profiles and image rejection tests."""
import copy
import hashlib
from pathlib import Path
import struct
import unittest
import update_profiles as p

SOURCE = Path(__file__).resolve().parent.parent


class ProfileTests(unittest.TestCase):
    def test_only_psram_release_profiles_are_enabled(self):
        profiles = p.supported_profiles(SOURCE)
        self.assertEqual(set(profiles), {'lilygo-t-embed-cc1101-plus', 'lilygo-t-embed-cc1101', 'esp32s3-psram', 'esp32p4-psram', 'esp32p4es-psram', 'esp32c5-psram', 'lilygo-t-embed-cc1101-plus-display', 'lilygo-t-embed-cc1101-plus-ui', 'lilygo-t-embed-cc1101-plus-ui-preview'})
        for env in ('esp32dev', 'esp32s3', 'esp32s3dbg', 'esp32c3', 'esp32c5', 'esp32c6', 'esp32p4', 'esp32p4es', 'native'):
            self.assertIsNone(p.load_profile(SOURCE, env))

    def test_plus_variants_declare_distinct_display_and_command_capabilities(self):
        expected = {
            'lilygo-t-embed-cc1101-plus': (False, False, False),
            'lilygo-t-embed-cc1101-plus-display': (True, False, False),
            'lilygo-t-embed-cc1101-plus-ui': (True, True, True),
            'lilygo-t-embed-cc1101-plus-ui-preview': (True, True, False),
        }
        switches = ('ESPSOMFY_STATUS_DISPLAY', 'ESPSOMFY_LOCAL_UI', 'ESPSOMFY_LOCAL_COMMANDS')
        for environment, enabled in expected.items():
            with self.subTest(environment=environment):
                profile = p.load_profile(SOURCE, environment)
                self.assertEqual(profile['board'], environment)
                self.assertIn('ESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS', profile['required_defines'])
                for flag, active in zip(switches, enabled):
                    self.assertIn(flag, profile['required_defines'] if active else profile['forbidden_defines'])
                if enabled[0]:
                    self.assertIn('adafruit/Adafruit GFX Library@1.12.1', profile['build_settings']['lib_deps'])
                    self.assertIn('adafruit/Adafruit BusIO@1.17.4', profile['build_settings']['lib_deps'])

    def test_capability_policy_must_be_complete_and_unambiguous(self):
        profile = p.load_profile(SOURCE, 'lilygo-t-embed-cc1101-plus')
        incomplete = copy.deepcopy(profile)
        incomplete['forbidden_defines'] = []
        with self.assertRaisesRegex(ValueError, 'feature'):
            p.validate_profile(incomplete)
        contradictory = copy.deepcopy(profile)
        contradictory['required_defines']['ESPSOMFY_LOCAL_COMMANDS'] = ''
        with self.assertRaisesRegex(ValueError, 'feature'):
            p.validate_profile(contradictory)

    def test_standard_lilygo_and_plus_have_distinct_board_ids(self):
        plus = p.load_profile(SOURCE, 'lilygo-t-embed-cc1101-plus')
        standard = p.load_profile(SOURCE, 'lilygo-t-embed-cc1101')
        self.assertNotEqual(plus['board'], standard['board'])
        self.assertEqual(plus['app0_address'], standard['app0_address'])
        self.assertEqual(standard['psram_bytes'], 0x800000)
        self.assertEqual(standard['bootloader_address'], 0)

    def test_p4_release_profiles_are_explicit_layout_opt_ins(self):
        for env, flash, app_bytes, app1, fs_address in (
                ('esp32p4-psram', 0x1000000, 0x330000, 0x340000, 0x670000),
                ('esp32p4es-psram', 0x400000, 0x1c0000, 0x1d0000, 0x390000)):
            profile = p.load_profile(SOURCE, env)
            self.assertEqual(profile['flash_bytes'], flash)
            self.assertEqual(profile['app_bytes'], app_bytes)
            self.assertEqual(profile['app1_address'], app1)
            self.assertEqual(profile['fs_address'], fs_address)
            self.assertEqual(profile['chip_id'], 18)
        self.assertIsNone(p.load_profile(SOURCE, 'esp32p4'))
        self.assertIsNone(p.load_profile(SOURCE, 'esp32p4es'))

    def test_c5_psram_profile_does_not_change_legacy_target(self):
        profile = p.load_profile(SOURCE, 'esp32c5-psram')
        self.assertEqual(profile['chip_id'], 23)
        self.assertEqual(profile['bootloader_address'], 0x2000)
        self.assertEqual(profile['flash_bytes'], 0x800000)
        self.assertEqual(profile['psram_bytes'], 0x800000)
        self.assertEqual(profile['app_bytes'], 0x330000)
        self.assertEqual(profile['sdk_memory'], 'qio_qspi')
        self.assertIsNone(p.load_profile(SOURCE, 'esp32c5'))

    def test_profile_define_contract_is_complete(self):
        defines = dict(p.profile_defines(p.load_profile(SOURCE, 'esp32p4-psram')))
        self.assertEqual(defines['ESPSOMFY_UPDATE_PROFILE'], 'esp32p4-psram')
        self.assertEqual(defines['ESPSOMFY_UPDATE_APP0_ADDRESS'], 0x10000)
        self.assertEqual(defines['ESPSOMFY_UPDATE_CHIP_ID'], 18)
        self.assertEqual(defines['ESPSOMFY_UPDATE_BOOTLOADER_ADDRESS'], 0x2000)

    def test_invalid_overlapping_regions_and_ram_are_rejected(self):
        profile = p.load_profile(SOURCE, 'esp32p4-psram')
        for changes in ({'app1_address': 0x10000}, {'psram_bytes': 0}, {'flash_bytes': 0x100000}, {'board': '../escape'}):
            with self.assertRaises(ValueError):
                p.validate_profile(dict(profile, **changes))

    def image(self, chip=18, flash=0x4f):
        header = bytearray(24)
        header[:4] = bytes((0xe9, 1, 2, flash))
        header[23] = 1
        struct.pack_into('<H', header, 12, chip)
        image = header + struct.pack('<II', 0x4ff00000, 7) + b'payload'
        checksum = 0xef
        for byte in b'payload': checksum ^= byte
        image.extend(b'\0' * ((-len(image) - 1) % 16) + bytes((checksum,)))
        image.extend(hashlib.sha256(image).digest())
        return image

    def test_image_validation_accepts_p4_and_s3_without_conflating_chip(self):
        p.validate_image(self.image(), p.load_profile(SOURCE, 'esp32p4-psram'))
        p.validate_image(self.image(chip=9), p.load_profile(SOURCE, 'esp32s3-psram'))
        p.validate_image(self.image(flash=0x2f), p.load_profile(SOURCE, 'esp32p4es-psram'))
        p.validate_image(self.image(chip=23, flash=0x3f), p.load_profile(SOURCE, 'esp32c5-psram'))
        with self.assertRaisesRegex(ValueError, 'chip'):
            p.validate_image(self.image(chip=9), p.load_profile(SOURCE, 'esp32p4-psram'))
        with self.assertRaisesRegex(ValueError, 'flash'):
            p.validate_image(self.image(flash=0x4f), p.load_profile(SOURCE, 'esp32p4es-psram'))

    def test_image_corruption_and_truncation_rejected(self):
        profile = p.load_profile(SOURCE, 'esp32p4-psram')
        bad = self.image(); bad[33] ^= 1
        for data in (bad, self.image()[:-1], b'not an ESP image'):
            with self.assertRaises(ValueError): p.validate_image(data, profile)


if __name__ == '__main__': unittest.main()

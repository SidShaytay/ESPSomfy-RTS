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
        self.assertEqual(set(profiles), {'lilygo-t-embed-cc1101-plus', 'lilygo-t-embed-cc1101', 'esp32s3-psram', 'esp32p4', 'esp32p4es', 'esp32c5-psram'})
        for env in ('esp32dev', 'esp32s3', 'esp32s3dbg', 'esp32c3', 'esp32c5', 'esp32c6', 'native'):
            self.assertIsNone(p.load_profile(SOURCE, env))

    def test_standard_lilygo_and_plus_have_distinct_board_ids(self):
        plus = p.load_profile(SOURCE, 'lilygo-t-embed-cc1101-plus')
        standard = p.load_profile(SOURCE, 'lilygo-t-embed-cc1101')
        self.assertNotEqual(plus['board'], standard['board'])
        self.assertEqual(plus['app0_address'], standard['app0_address'])
        self.assertEqual(standard['psram_bytes'], 0x800000)
        self.assertEqual(standard['bootloader_address'], 0)

    def test_p4_profiles_preserve_existing_small_layout(self):
        for env, flash in (('esp32p4', 0x1000000), ('esp32p4es', 0x400000)):
            profile = p.load_profile(SOURCE, env)
            self.assertEqual(profile['flash_bytes'], flash)
            self.assertEqual(profile['app_bytes'], 0x180000)
            self.assertEqual(profile['app1_address'], 0x190000)
            self.assertEqual(profile['fs_address'], 0x310000)
            self.assertEqual(profile['chip_id'], 18)

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
        defines = dict(p.profile_defines(p.load_profile(SOURCE, 'esp32p4')))
        self.assertEqual(defines['ESPSOMFY_UPDATE_PROFILE'], 'esp32p4')
        self.assertEqual(defines['ESPSOMFY_UPDATE_APP0_ADDRESS'], 0x10000)
        self.assertEqual(defines['ESPSOMFY_UPDATE_CHIP_ID'], 18)
        self.assertEqual(defines['ESPSOMFY_UPDATE_BOOTLOADER_ADDRESS'], 0x2000)

    def test_invalid_overlapping_regions_and_ram_are_rejected(self):
        profile = p.load_profile(SOURCE, 'esp32p4')
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
        p.validate_image(self.image(), p.load_profile(SOURCE, 'esp32p4'))
        p.validate_image(self.image(chip=9), p.load_profile(SOURCE, 'esp32s3-psram'))
        p.validate_image(self.image(flash=0x2f), p.load_profile(SOURCE, 'esp32p4es'))
        p.validate_image(self.image(chip=23, flash=0x3f), p.load_profile(SOURCE, 'esp32c5-psram'))
        with self.assertRaisesRegex(ValueError, 'chip'):
            p.validate_image(self.image(chip=9), p.load_profile(SOURCE, 'esp32p4'))
        with self.assertRaisesRegex(ValueError, 'flash'):
            p.validate_image(self.image(flash=0x4f), p.load_profile(SOURCE, 'esp32p4es'))

    def test_image_corruption_and_truncation_rejected(self):
        profile = p.load_profile(SOURCE, 'esp32p4')
        bad = self.image(); bad[33] ^= 1
        for data in (bad, self.image()[:-1], b'not an ESP image'):
            with self.assertRaises(ValueError): p.validate_image(data, profile)


if __name__ == '__main__': unittest.main()

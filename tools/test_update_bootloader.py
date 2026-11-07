"""Offline bootloader identity generation; never reads a controller."""
import hashlib
from pathlib import Path
import tempfile
import unittest
from test_lilygo_manifest import ManifestTests
import update_bootloader as b
from update_profiles import load_profile


class BootloaderTests(ManifestTests):
    def setUp(self):
        super().setUp()
        self.sdk_tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.sdk_tmp.cleanup)
        self.sdkconfig = Path(self.sdk_tmp.name) / 'sdkconfig.h'
        self.sdkconfig.write_text('#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1\n#define CONFIG_BOOTLOADER_OFFSET_IN_FLASH 0\n')

    def test_hashes_exact_bootloader_bytes(self):
        image = self.out / 'bootloader.bin'
        text = b.expected_header(image, self.sdkconfig)
        self.assertIn(f'ESPSOMFY_BOOTLOADER_BYTES = {image.stat().st_size};', text)
        digest = hashlib.sha256(image.read_bytes()).digest()
        self.assertIn(', '.join(f'0x{byte:02x}' for byte in digest), text)
        self.assertIn('ESPSOMFY_BOOTLOADER_SHA256_HEX[] = \"' + digest.hex() + '\";', text)
        self.assertIn(f'ESPSOMFY_BOOT_ID:0:{image.stat().st_size}:{digest.hex()}', text)

    def test_p4_bootloader_uses_its_own_chip_contract(self):
        profile = load_profile(Path(__file__).resolve().parent.parent, 'esp32p4-psram')
        self.sdkconfig.write_text('#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1\n#define CONFIG_BOOTLOADER_OFFSET_IN_FLASH 0x2000\n')
        with self.assertRaisesRegex(ValueError, 'chip'):
            b.expected_header(self.out / 'bootloader.bin', self.sdkconfig, profile)

    def test_rollback_config_missing_or_disabled_is_rejected(self):
        for text in ('', '#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 0\n', '#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1\n#undef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE\n'):
            self.sdkconfig.write_text(text)
            with self.assertRaisesRegex(ValueError, 'rollback'):
                b.expected_header(self.out / 'bootloader.bin', self.sdkconfig)

    def test_bootloader_address_is_bound_to_sdk_configuration(self):
        self.sdkconfig.write_text('#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1\n#define CONFIG_BOOTLOADER_OFFSET_IN_FLASH 0x2000\n')
        with self.assertRaisesRegex(ValueError, 'offset'):
            b.expected_header(self.out / 'bootloader.bin', self.sdkconfig)

    def test_corrupt_bootloader_image_is_rejected(self):
        (self.out / 'bootloader.bin').write_bytes(b'corrupt')
        with self.assertRaises(ValueError):
            b.expected_header(self.out / 'bootloader.bin', self.sdkconfig)

    def test_oversized_bootloader_does_not_overlap_partitions(self):
        (self.out / 'bootloader.bin').write_bytes(b'\0' * 0x8001)
        with self.assertRaisesRegex(ValueError, 'partition'):
            b.expected_header(self.out / 'bootloader.bin', self.sdkconfig)


if __name__ == '__main__':
    unittest.main()

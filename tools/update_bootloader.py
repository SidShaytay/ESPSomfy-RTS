"""Bind application rollback trust to the exact generated profile bootloader."""
import hashlib
from pathlib import Path
import re
from update_profiles import load_profile
from lilygo_manifest import image_header


def expected_header(bootloader, sdkconfig, profile=None):
    if profile is None:
        profile = load_profile(Path(__file__).resolve().parent.parent, 'lilygo-t-embed-cc1101')
    config = sdkconfig.read_text()
    defines = re.findall(r'^\s*#\s*define\s+CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE\s+(\S+)', config, re.MULTILINE)
    if defines != ['1'] or re.search(r'^\s*#\s*undef\s+CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE\b', config, re.MULTILINE):
        raise ValueError('pinned SDK bootloader rollback support is missing or disabled')
    offsets = re.findall(r'^\s*#\s*define\s+CONFIG_BOOTLOADER_OFFSET_IN_FLASH\s+(\S+)', config, re.MULTILINE)
    if (len(offsets) != 1 or int(offsets[0], 0) != profile['bootloader_address'] or
            re.search(r'^\s*#\s*undef\s+CONFIG_BOOTLOADER_OFFSET_IN_FLASH\b',config,re.MULTILINE)):
        raise ValueError('pinned SDK bootloader offset differs from update profile')
    data = bootloader.read_bytes()
    if not data or len(data) > 0x8000 - profile['bootloader_address']:
        raise ValueError('bootloader overlaps the partition table')
    image_header(data,"bootloader.bin")
    digest = ', '.join(f'0x{byte:02x}' for byte in hashlib.sha256(data).digest())
    return ('// Generated from the actual build bootloader; never edit or commit.\n'
            '#pragma once\n#include <cstddef>\n#include <cstdint>\n'
            f'constexpr uint32_t ESPSOMFY_BOOTLOADER_ADDRESS = {profile["bootloader_address"]};\n'
            f'constexpr size_t ESPSOMFY_BOOTLOADER_BYTES = {len(data)};\n'
            f'constexpr uint8_t ESPSOMFY_BOOTLOADER_SHA256[32] = {{{digest}}};\n'
            f'#define ESPSOMFY_BOOTLOADER_ID \"ESPSOMFY_BOOT_ID:{profile["bootloader_address"]}:{len(data)}:{hashlib.sha256(data).hexdigest()}\"\n'
            f'constexpr char ESPSOMFY_BOOTLOADER_SHA256_HEX[] = \"{hashlib.sha256(data).hexdigest()}\";\n')


def configure(env):
    profile = load_profile(Path(env.subst('$PROJECT_DIR')), env.get('PIOENV'))
    if profile is None:
        return
    build = Path(env.subst('$BUILD_DIR'))
    libs = Path(env.PioPlatform().get_package_dir('framework-arduinoespressif32-libs'))
    sdkconfig = libs / 'esp32s3' / 'qio_opi' / 'include' / 'sdkconfig.h'
    bootloader = build / 'bootloader.bin'
    header = build / 'GitUpdateBootloaderExpected.h'
    obj = '$BUILD_DIR/src/GitUpdateBootloader.cpp.o'

    def generate(source, target, env):
        text = expected_header(bootloader, sdkconfig, profile)
        if not header.exists() or header.read_text() != text:
            header.write_text(text)

    env.Append(CPPPATH=[str(build)], LINKFLAGS=['-Wl,--undefined=espsomfyBootloaderIdentity'])
    # Pioarduino generates bootloader.bin from the pinned SDK ELF. Force that
    # exact conversion to finish before hashing/compiling the expected digest.
    env.Depends(obj, [str(bootloader), str(sdkconfig), __file__])
    env.AddPreAction(obj, generate)

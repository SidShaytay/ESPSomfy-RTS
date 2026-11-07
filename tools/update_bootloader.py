"""Bind application rollback trust to the exact generated Plus bootloader."""
import hashlib
from pathlib import Path
import re
from lilygo_manifest import image_header


def expected_header(bootloader, sdkconfig):
    config = sdkconfig.read_text()
    defines = re.findall(r'^\s*#\s*define\s+CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE\s+(\S+)', config, re.MULTILINE)
    if defines != ['1'] or re.search(r'^\s*#\s*undef\s+CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE\b', config, re.MULTILINE):
        raise ValueError('pinned SDK bootloader rollback support is missing or disabled')
    data = bootloader.read_bytes()
    if not data or len(data) > 0x8000:
        raise ValueError('bootloader overlaps the partition table')
    image_header(data, 'bootloader.bin')
    digest = ', '.join(f'0x{byte:02x}' for byte in hashlib.sha256(data).digest())
    return ('// Generated from the actual build bootloader; never edit or commit.\n'
            '#pragma once\n#include <cstddef>\n#include <cstdint>\n'
            f'constexpr size_t ESPSOMFY_BOOTLOADER_BYTES = {len(data)};\n'
            f'constexpr uint8_t ESPSOMFY_BOOTLOADER_SHA256[32] = {{{digest}}};\n'
            f'constexpr char ESPSOMFY_BOOTLOADER_SHA256_HEX[] = \"{hashlib.sha256(data).hexdigest()}\";\n')


def configure(env):
    if env.get('PIOENV') != 'lilygo-t-embed-cc1101-plus':
        return
    build = Path(env.subst('$BUILD_DIR'))
    libs = Path(env.PioPlatform().get_package_dir('framework-arduinoespressif32-libs'))
    sdkconfig = libs / 'esp32s3' / 'qio_opi' / 'include' / 'sdkconfig.h'
    bootloader = build / 'bootloader.bin'
    header = build / 'GitUpdateBootloaderExpected.h'
    obj = '$BUILD_DIR/src/GitUpdateBootloader.cpp.o'

    def generate(source, target, env):
        text = expected_header(bootloader, sdkconfig)
        if not header.exists() or header.read_text() != text:
            header.write_text(text)

    env.Append(CPPPATH=[str(build)])
    # Pioarduino generates bootloader.bin from the pinned SDK ELF. Force that
    # exact conversion to finish before hashing/compiling the expected digest.
    env.Depends(obj, [str(bootloader), str(sdkconfig), __file__])
    env.AddPreAction(obj, generate)

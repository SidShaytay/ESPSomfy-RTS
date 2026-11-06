#!/usr/bin/env python3
"""Create/check a Plus artifact package offline. Never opens a serial port.

This is integrity and target validation, not a signature or hardware detection.
A firmware update is valid only after separate provisioning of this exact table.
"""
import argparse
import re
import shlex
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ENV = 'lilygo-t-embed-cc1101-plus'
BASELINE = {'repository': 'https://github.com/cjkas/ESPSomfy-RTS',
            'tag': 'v4.0.2', 'revision': '07bffdec6b540cc99f31d02d7e6de6d3d1e7f8f3'}
PLATFORM = 'https://github.com/pioarduino/platform-espressif32/releases/download/55.03.312/platform-espressif32.zip'
TARGET = {'board': 'LILYGO T-Embed CC1101 Plus', 'chip': 'esp32s3',
          'environment': ENV, 'flash_bytes': 0x1000000, 'flash_mode': 'qio',
          'flash_frequency_hz': 80000000, 'rom_header_flash_mode': 'dio',
          'psram_bytes': 0x800000,
          'psram_mode': 'opi', 'usb': 'hardware CDC on boot', 'headless': True,
          'profile_macro': 'ESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS'}
BUILD_SETTINGS = {'board': 'esp32-s3-devkitc1-n16r8', 'build_type': 'release',
                  'board_build.partitions': 'default_8MB.csv',
                  'board_build.flash_mode': 'qio', 'board_build.f_flash': '80000000L',
                  'board_build.arduino.memory_type': 'qio_opi',
                  'board_build.psram_type': 'opi', 'board_upload.flash_size': '16MB'}
REQUIRED_FLAGS = ['-DCORE_DEBUG_LEVEL=2', '-DARDUINO_USB_MODE=1',
                  '-DARDUINO_USB_CDC_ON_BOOT=1',
                  '-DESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS']
PARTITIONS = {
    'nvs': (1, 2, 0x9000, 0x5000),
    'otadata': (1, 0, 0xe000, 0x2000),
    'app0': (0, 0x10, 0x10000, 0x330000),
    'app1': (0, 0x11, 0x340000, 0x330000),
    'spiffs': (1, 0x82, 0x670000, 0x180000),
    'coredump': (1, 3, 0x7f0000, 0x10000),
}
SEGMENTS = [('bootloader.bin', 0, 0x8000),
            ('partitions.bin', 0x8000, 0x1000),
            ('boot_app0.bin', 0xe000, 0x2000),
            ('firmware.bin', 0x10000, 0x330000),
            ('littlefs.bin', 0x670000, 0x180000)]
USAGE = {'provisioning': [name for name, _, _ in SEGMENTS],
         'firmware_update': ['firmware.bin'], 'filesystem_update': ['littlefs.bin'],
         'requires_matching_partition_table': True,
         'migration_from_older_layout': 'separate backup, provisioning and restore required; never firmware OTA'}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def partition_table(data):
    if len(data) != 0xc00:
        raise ValueError('partition table must be 3072 bytes')
    found = {}
    checked_md5 = False
    for start in range(0, len(data), 32):
        row = data[start:start + 32]
        if row == b'\xff' * 32:
            if any(b != 0xff for b in data[start:]):
                raise ValueError('non-padding after table terminator')
            break
        if row[:2] == b'\xeb\xeb':
            if checked_md5 or row[2:16] != b'\xff' * 14 or row[16:] != hashlib.md5(data[:start]).digest():
                raise ValueError('partition table MD5 mismatch')
            checked_md5 = True
            continue
        if checked_md5:
            raise ValueError('partition after table MD5')
        magic, kind, subtype, offset, size, name, flags = struct.unpack('<HBBII16sI', row)
        if magic != 0x50aa or flags != 0:
            raise ValueError('invalid partition record')
        label = name.rstrip(b'\0').decode('ascii')
        if label in found:
            raise ValueError('duplicate partition')
        found[label] = (kind, subtype, offset, size)
    if not checked_md5 or found != PARTITIONS:
        raise ValueError('not the Plus partition layout')
    return {name: {'type': values[0], 'subtype': values[1],
                   'offset': values[2], 'size': values[3]}
            for name, values in found.items()}


def image_header(data, filename):
    if len(data) < 24 or data[0] != 0xe9 or not 1 <= data[1] <= 16:
        raise ValueError(f'{filename}: invalid ESP image header')
    if struct.unpack_from('<H', data, 12)[0] != 9:
        raise ValueError(f'{filename}: not ESP32-S3')
    # The ROM starts in DIO; this QIO SDK bootloader enables quad mode later.
    # https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/spi-flash-modes.html
    if data[2] != 2 or data[3] != 0x4f:
        raise ValueError(f'{filename}: expected ROM DIO, 16MB, 80MHz header')
    position, checksum = 24, 0xef
    for _ in range(data[1]):
        if position + 8 > len(data):
            raise ValueError(f'{filename}: truncated segment header')
        size = struct.unpack_from('<I', data, position + 4)[0]
        position += 8
        if position + size > len(data):
            raise ValueError(f'{filename}: truncated segment')
        for byte in data[position:position + size]:
            checksum ^= byte
        position += size
    end = (position + 16) // 16 * 16
    if end > len(data) or data[end - 1] != checksum:
        raise ValueError(f'{filename}: invalid image checksum')
    if data[23] != 1 or len(data) != end + 32 or data[end:] != hashlib.sha256(data[:end]).digest():
        raise ValueError(f'{filename}: invalid appended image SHA256')


def build_configuration(profile, compiler):
    """Validate resolved PIO settings and the compiler's final macro set."""
    for key, expected in BUILD_SETTINGS.items():
        if profile.get(key) != expected:
            raise ValueError(f'Plus build setting {key} does not match {expected}')
    if profile.get('platform') != PLATFORM or profile.get('framework') != ['arduino']:
        raise ValueError('not the pinned Arduino platform')
    if profile.get('build_unflags'):
        raise ValueError('Plus profile does not permit build_unflags')
    if compiler.get('build_type') != 'release' or compiler.get('env_name') != ENV:
        raise ValueError('compiler metadata is not the Plus release environment')
    required = dict(flag[2:].partition('=')[::2] for flag in REQUIRED_FLAGS)
    required['BOARD_HAS_PSRAM'] = ''
    required['IDF_VER'] = '"v5.5.5"'
    defines = compiler.get('defines', [])
    for name, value in required.items():
        found = [entry.partition('=')[2] for entry in defines
                 if entry.partition('=')[0] == name]
        if found != [value]:
            raise ValueError(f'effective compiler macro {name} is missing or conflicting')
    # -U may survive as a raw C/C++ flag; don't let a later undefine defeat
    # CPPDEFINES. Conflicting -D values are rejected here as well.
    for field in ('cc_flags', 'cxx_flags'):
        tokens = shlex.split(' '.join(compiler.get(field, [])))
        for index, flag in enumerate(tokens):
            if flag in ('-D', '-U') and index + 1 < len(tokens):
                flag += tokens[index + 1]
            if flag.startswith(('-D', '-U')):
                name, _, value = flag[2:].partition('=')
                if name in required and (flag.startswith('-U') or value != required[name]):
                    raise ValueError(f'{field} overrides required macro {name}')
    includes = compiler.get('includes', {}).get('build', [])
    if not any(path.replace('\\', '/').endswith('/esp32s3/qio_opi/include') for path in includes):
        raise ValueError('compiler SDK does not use ESP32-S3 QIO/OPI memory')
    return {'resolved': profile, 'compiler': compiler}


def build_evidence(out):
    # Keep only target compiler evidence: unrelated upload target descriptions
    # and other environments are not inputs to the package contract.
    resolved = dict(json.loads((out / 'resolved-config.json').read_text()))
    profile = dict(resolved['env:' + ENV])
    metadata = json.loads((out / 'compiler-metadata.json').read_text())[ENV]
    compiler = {key: metadata[key] for key in
                ('env_name', 'build_type', 'defines', 'cc_flags', 'cxx_flags', 'includes')}
    return build_configuration(profile, compiler)


def artifact_manifest(out):
    """Derive sizes/hashes from bytes, rejecting incompatible artifacts first."""
    allowed = {name for name, _, _ in SEGMENTS} | {
        'firmware.elf', 'resolved-config.json', 'compiler-metadata.json', 'flash-manifest.json'}
    for path in out.iterdir():
        if path.name not in allowed or not path.is_file() or path.is_symlink():
            raise ValueError(f'unexpected artifact package entry: {path.name}')
    table = partition_table((out / 'partitions.bin').read_bytes())
    segments = []
    for name, offset, limit in SEGMENTS:
        data = (out / name).read_bytes()
        if not data or len(data) > limit:
            raise ValueError(f'{name}: image exceeds reserved region or is empty')
        if name == 'littlefs.bin' and len(data) != limit:
            raise ValueError('filesystem image must exactly fill its partition')
        if name == 'boot_app0.bin' and (len(data) != limit or digest(data) !=
                'f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0'):
            raise ValueError('OTA initialization image does not match the pinned SDK')
        if name in ('firmware.bin', 'bootloader.bin'):
            image_header(data, name)
        if name == 'firmware.bin' and limit - len(data) < 8192:
            raise ValueError('firmware has less than 8KB OTA headroom')
        segments.append({'file': name, 'offset': offset, 'size': len(data),
                         'sha256': digest(data)})
    elf = (out / 'firmware.elf').read_bytes()
    if not elf:
        raise ValueError('missing ELF symbols')
    return {'schema': 1, 'baseline': BASELINE.copy(), 'target': TARGET.copy(),
            'platform': PLATFORM, 'arduino_core': '3.3.12', 'esp_idf': '5.5.5',
            'pioarduino_core': '6.2.0', 'build_settings': BUILD_SETTINGS.copy(),
            'profile_build_flags': list(REQUIRED_FLAGS), 'effective_build': build_evidence(out),
            'partitions': table, 'segments': segments,
            'usage': dict(USAGE),
            'symbols': {'file': 'firmware.elf', 'size': len(elf), 'sha256': digest(elf)}}


def validate(manifest, out):
    # Compare the complete derived contract, including exact filenames/offsets,
    # before accepting caller-supplied file paths or target declarations.
    expected = artifact_manifest(out)
    for key, value in expected.items():
        if manifest.get(key) != value:
            raise ValueError(f'manifest {key} does not match the Plus artifacts/contract')
    source = manifest.get('source', {})
    if (not re.fullmatch(r'[0-9a-f]{40}', source.get('revision', '')) or
            type(source.get('dirty')) is not bool or
            not isinstance(source.get('input_sha256'), dict) or
            'platformio.ini' not in source['input_sha256']):
        raise ValueError('missing exact source provenance')
    for name, value in source['input_sha256'].items():
        if (not isinstance(name, str) or Path(name).is_absolute() or '..' in Path(name).parts or
                not isinstance(value, str) or not re.fullmatch(r'[0-9a-f]{64}', value)):
            raise ValueError('invalid source hash record')
    return manifest


def source_record(source):
    def git(*args):
        return subprocess.check_output(['git', '-C', str(source), *args], text=True).strip()
    subprocess.run(['git', '-C', str(source), 'merge-base', '--is-ancestor',
                    BASELINE['revision'], 'HEAD'], check=True)
    inputs = [*source.glob('*.py'), source / 'platformio.ini']
    for directory in ('src', 'lib', 'data-src', 'tools'):
        inputs.extend(p for p in (source / directory).rglob('*')
                      if p.is_file() and '__pycache__' not in p.parts)
    hashes = {str(p.relative_to(source)): digest(p.read_bytes()) for p in sorted(inputs)}
    return {'revision': git('rev-parse', 'HEAD'),
            'dirty': bool(git('status', '--porcelain', '--untracked-files=normal')),
            'input_sha256': hashes}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['create', 'validate'])
    parser.add_argument('directory', type=Path)
    parser.add_argument('--source', type=Path)
    args = parser.parse_args()
    path = args.directory / 'flash-manifest.json'
    if args.command == 'create':
        if not args.source:
            parser.error('create requires --source')
        path.unlink(missing_ok=True)
        manifest = artifact_manifest(args.directory)
        manifest['source'] = source_record(args.source.resolve())
        validate(manifest, args.directory)
        path.write_text(json.dumps(manifest, indent=2) + '\n')
    else:
        validate(json.loads(path.read_text()), args.directory)
    print(f'Validated Plus artifacts: {path}')


if __name__ == '__main__':
    main()

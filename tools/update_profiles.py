"""Single source of exact-board, PSRAM, partition and compiler update contracts."""
import copy
import hashlib
import json
from pathlib import Path
import re
import struct

NUMBERS = ('flash_bytes', 'psram_bytes', 'chip_id', 'bootloader_address', 'app_bytes',
           'app0_address', 'app1_address', 'fs_address', 'fs_bytes', 'nvs_address', 'nvs_bytes')
DEFINE_FIELDS = {'PROFILE': 'board', 'LAYOUT': 'layout',
                 **{name.upper(): name for name in NUMBERS}}


def validate_profile(profile):
    for name in ('board', 'layout', 'chip', 'sdk_memory'):
        if not isinstance(profile.get(name), str) or not re.fullmatch(r'[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}', profile[name]):
            raise ValueError(f'invalid update profile {name}')
    for name in NUMBERS:
        if type(profile.get(name)) is not int or not 0 <= profile[name] <= 0xffffffff:
            raise ValueError(f'invalid update profile {name}')
    if profile['psram_bytes'] < 0x800000 or profile['flash_bytes'] not in (0x400000, 0x800000, 0x1000000, 0x2000000):
        raise ValueError('insufficient or unsupported profile memory')
    if (profile['chip'], profile['chip_id']) not in (('esp32s3', 9), ('esp32p4', 18), ('esp32p4_es', 18)):
        raise ValueError('unsupported update profile chip')
    regions = [(0x8000, 0x1000), (0xe000, 0x2000),
               (profile['nvs_address'], profile['nvs_bytes']),
               (profile['app0_address'], profile['app_bytes']),
               (profile['app1_address'], profile['app_bytes']),
               (profile['fs_address'], profile['fs_bytes'])]
    regions.sort()
    previous_end = 0
    for address, size in regions:
        if size <= 0 or address % 0x1000 or size % 0x1000 or address < previous_end or address + size > profile['flash_bytes']:
            raise ValueError('invalid or overlapping update profile partitions')
        previous_end = address + size
    if profile['bootloader_address'] != 0 or not isinstance(profile.get('build_settings'), dict):
        raise ValueError('invalid bootloader or compiler profile')
    if profile['build_settings'].get('build_type') != 'release':
        raise ValueError('updater requires a release build with two OTA slots')
    return profile


def supported_profiles(source):
    document = json.loads((Path(source) / 'update-profiles.json').read_text())
    if document.get('schema') != 1 or not isinstance(document.get('profiles'), dict):
        raise ValueError('invalid update profiles schema')
    for environment, profile in document['profiles'].items():
        validate_profile(profile)
        if environment != profile['board']:
            raise ValueError('profile board identity must match its explicit environment')
    return document['profiles']


def load_profile(source, environment):
    return copy.deepcopy(supported_profiles(source).get(environment))


def profile_defines(profile):
    validate_profile(profile)
    return [('ESPSOMFY_UPDATE_' + suffix, profile[field]) for suffix, field in DEFINE_FIELDS.items()]


def validate_image(data, profile):
    """Validate generic ESP image framing/checksum/hash and exact profile chip/flash."""
    if len(data) < 24 or data[0] != 0xe9 or not 1 <= data[1] <= 16:
        raise ValueError('invalid ESP image header')
    if struct.unpack_from('<H', data, 12)[0] != profile['chip_id']:
        raise ValueError('ESP image chip differs from update profile')
    flash_code = {0x400000: 2, 0x800000: 3, 0x1000000: 4, 0x2000000: 5}[profile['flash_bytes']]
    if data[3] >> 4 != flash_code or data[3] & 15 != 15 or data[2] not in (0, 2):
        raise ValueError('ESP image flash capacity/mode/frequency differs')
    position, checksum = 24, 0xef
    for _ in range(data[1]):
        if position + 8 > len(data): raise ValueError('truncated ESP segment header')
        size = struct.unpack_from('<I', data, position + 4)[0]
        position += 8
        if position + size > len(data): raise ValueError('truncated ESP segment')
        for byte in data[position:position + size]: checksum ^= byte
        position += size
    end = (position + 16) // 16 * 16
    if end > len(data) or data[end - 1] != checksum: raise ValueError('ESP image checksum mismatch')
    if data[23] != 1 or len(data) != end + 32 or data[end:] != hashlib.sha256(data[:end]).digest():
        raise ValueError('ESP image appended SHA256 mismatch')

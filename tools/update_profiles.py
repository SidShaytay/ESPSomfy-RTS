"""One reviewed shared hardware profile; no generic or retired fallback."""
import copy
import json
from pathlib import Path
from lilygo_manifest import ENV, TARGET


def supported_profiles(source):
    document = json.loads((Path(source) / 'update-profiles.json').read_text())
    expected = {'board': ENV, 'compatible_boards': TARGET['compatible_boards'],
                'layout': 'default_8MB', 'features': 15, 'assets_mode': 'bundled', 'flash_bytes': 0x1000000,
                'psram_bytes': 0x800000, 'app_bytes': 0x330000,
                'app0_address': 0x10000, 'app1_address': 0x340000,
                'fs_address': 0x670000, 'fs_bytes': 0x180000,
                'nvs_address': 0x9000, 'nvs_bytes': 0x5000,
                'bootloader_address': 0, 'chip_id': 9}
    if (type(document.get('schema')) is not int or document.get('schema') != 3 or document.get('profiles') != {ENV: expected} or
            any(type(document['profiles'][ENV].get(k)) is not type(v) for k,v in expected.items())):
        raise ValueError('unsupported or altered shared updater profile')
    return document['profiles']


def load_profile(source, environment):
    return copy.deepcopy(supported_profiles(source).get(environment))

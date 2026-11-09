"""Build identity shared by the compiler and offline release packager."""
import re
import subprocess

BOARD = 'lilygo-t-embed-cc1101'
LAYOUT = 'default_8MB'
MANIFEST_ASSET = 'update-' + BOARD + '.json'
FIRMWARE_ASSET = 'firmware-' + BOARD + '.bin'
STABLE_TAG = re.compile(r'v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)')


def stable_tag(tag):
    match = STABLE_TAG.fullmatch(tag)
    return bool(match and len(tag) <= 47 and all(int(part) <= 0xffffffff for part in match.groups()))


def git(source, *args):
    # Read-only local provenance; never authenticates or publishes.
    command = 'git'
    return subprocess.check_output([command, '-C', str(source), *args], text=True).strip()


def source_epoch(source):
    value = int(git(source, 'show', '-s', '--format=%ct', 'HEAD'))
    if not 1577836800 <= value <= 0x7fffffffffffffff:
        raise ValueError('invalid source timestamp for verified HTTPS clock floor')
    return value


def tags_at_head(source):
    return git(source, 'tag', '--points-at', 'HEAD').splitlines()


def identity(source, app_compat, tags, profile=None):
    profile = profile or {'board': BOARD, 'layout': LAYOUT, 'assets_mode': 'bundled'}
    if profile['board'] != BOARD or profile['layout'] != LAYOUT or profile.get('assets_mode') != 'bundled':
        raise ValueError('unsupported or retired update profile')
    if type(source.get('dirty')) is not bool:
        raise ValueError('missing source dirty state')
    for name in ('board', 'layout'):
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}', profile[name]):
            raise ValueError('invalid board or layout identity')
    if not re.fullmatch(r'[0-9a-f]{40}', source['revision']):
        raise ValueError('invalid source revision')
    if not re.fullmatch(r'sha256:[0-9a-f]{64}', app_compat):
        raise ValueError('invalid packaged application compatibility digest')
    stable = [tag for tag in tags if stable_tag(tag)]
    version = stable[0] if not source['dirty'] and len(stable) == 1 else (
        'dev-' + source['revision'][:12] + ('-dirty' if source['dirty'] else ''))
    marker = '|'.join(('ESPSOMFY_UPDATE_ID:3', profile['board'], profile['layout'], source['revision'], version, profile['assets_mode'], app_compat))
    return {'schema': 3, 'assets_mode': profile['assets_mode'], 'board': profile['board'], 'layout': profile['layout'],
            'source_sha': source['revision'], 'version': version,
            'app_compat': app_compat, 'marker': marker}

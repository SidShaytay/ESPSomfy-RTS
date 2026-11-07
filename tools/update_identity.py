"""Build identity shared by the compiler and offline release packager."""
import re
import shutil
import subprocess

BOARD = 'lilygo-t-embed-cc1101-plus'
LAYOUT = 'default_8MB'
MANIFEST_ASSET = 'update-' + BOARD + '.json'
FIRMWARE_ASSET = 'firmware-' + BOARD + '.bin'
STABLE_TAG = re.compile(r'v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)')


def git(source, *args):
    # Sid's authenticated wrapper is used locally; stock git is for CI hosts.
    command = 'git-agent' if shutil.which('git-agent') else 'git'
    return subprocess.check_output([command, '-C', str(source), *args], text=True).strip()


def tags_at_head(source):
    return git(source, 'tag', '--points-at', 'HEAD').splitlines()


def identity(source, app_compat, tags):
    if not re.fullmatch(r'[0-9a-f]{40}', source['revision']):
        raise ValueError('invalid source revision')
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]{0,30}', app_compat):
        raise ValueError('invalid application compatibility version')
    stable = [tag for tag in tags if STABLE_TAG.fullmatch(tag)]
    version = stable[0] if not source['dirty'] and len(stable) == 1 else (
        'dev-' + source['revision'][:12] + ('-dirty' if source['dirty'] else ''))
    marker = '|'.join(('ESPSOMFY_UPDATE_ID:1', BOARD, LAYOUT, source['revision'], version, app_compat))
    return {'schema': 1, 'board': BOARD, 'layout': LAYOUT,
            'source_sha': source['revision'], 'version': version,
            'app_compat': app_compat, 'marker': marker}

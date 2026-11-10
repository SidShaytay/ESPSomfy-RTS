"""Canonical compatibility identity for the exact packaged LittleFS web files."""
import hashlib
from pathlib import Path
import re

FILENAME = 'update-assets.txt'
HEADER = 'ESPSOMFY_ASSETS_V1\n'
MAX_FILES = 128
MAX_MANIFEST = 16384
MAX_FILE = 0x180000


def safe_path(name):
    return (len(name) <= 127 and name.startswith('/') and
            all(re.fullmatch(r'[A-Za-z0-9_][A-Za-z0-9._-]*', part)
                for part in name[1:].split('/')))


def manifest_bytes(directory):
    directory = Path(directory)
    lines = []
    total = 0
    for path in sorted(directory.rglob('*'), key=lambda p: p.relative_to(directory).as_posix()):
        if path.is_symlink():
            raise ValueError('web assets must not be symlinks')
        if not path.is_file() or path.relative_to(directory).as_posix() == FILENAME:
            continue
        name = '/' + path.relative_to(directory).as_posix()
        if not safe_path(name):
            raise ValueError('unsafe web asset path')
        contents = path.read_bytes()
        total += len(contents)
        if total > MAX_FILE:
            raise ValueError('web asset exceeds filesystem capacity')
        lines.append(f'{hashlib.sha256(contents).hexdigest()} {len(contents)} {name}\n')
    if not lines or len(lines) > MAX_FILES:
        raise ValueError('unsupported web asset count')
    data = (HEADER + ''.join(lines)).encode('ascii')
    if len(data) > MAX_MANIFEST:
        raise ValueError('web asset manifest exceeds runtime bound')
    return data


def compatibility(data):
    return 'sha256:' + hashlib.sha256(data).hexdigest()


def write_manifest(directory):
    directory = Path(directory)
    data = manifest_bytes(directory)
    (directory / FILENAME).write_bytes(data)
    return compatibility(data)


def verify_manifest(directory):
    directory = Path(directory)
    data = (directory / FILENAME).read_bytes()
    if data != manifest_bytes(directory):
        raise ValueError('packaged web asset manifest is stale or corrupt')
    return compatibility(data)

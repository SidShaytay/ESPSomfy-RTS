"""Build and verify the exact immutable web bytes inside canonical firmware."""
import hashlib
import json
from pathlib import Path
import re
import struct
import tempfile
from html.parser import HTMLParser
import zlib
from update_assets import (FILENAME, HEADER, MAX_FILES, MAX_FILE, MAX_MANIFEST,
                           compatibility, verify_manifest)

MAGIC = b'ESPSOMFY_WEB_BUNDLE_V1\0'
MIME = {'.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css',
        '.png': 'image/png', '.svg': 'image/svg+xml', '.ico': 'image/x-icon'}
MAX_EXPANDED = 8 * 1024 * 1024
REQUIRED = {'/index.html', '/index.js', '/app.css', '/appversion'}


class WebMode(HTMLParser):
    def __init__(self):
        super().__init__(); self.modes=[]
    def handle_starttag(self,tag,attributes):
        if tag!='meta':return
        names=[value for key,value in attributes if key=='name']
        if not any(value and value.lower()=='espsomfy-web-mode' for value in names):return
        contents=[value for key,value in attributes if key=='content']
        if len(names)!=1 or len(contents)!=1:
            raise ValueError('ambiguous browser web-mode attributes')
        self.modes.append(contents[0])


def records(manifest):
    if not manifest or len(manifest) > MAX_MANIFEST:
        raise ValueError('bundled manifest size invalid')
    try:
        text = manifest.decode('ascii')
    except UnicodeError as error:
        raise ValueError('bundled manifest is not ASCII') from error
    if not text.startswith(HEADER) or not text.endswith('\n'):
        raise ValueError('invalid bundled manifest header')
    result, public, previous, total = [], set(), '', 0
    for line in text[len(HEADER):].splitlines():
        match = re.fullmatch(r'([0-9a-f]{64}) (0|[1-9][0-9]*) /([A-Za-z0-9_][A-Za-z0-9._-]*)', line)
        if not match:
            raise ValueError('unsafe or non-root bundled asset')
        digest, size, name = match.group(1), int(match.group(2)), match.group(3)
        if len(name) + 1 > 127 or name <= previous or '..' in name or size > MAX_FILE - total:
            raise ValueError('bundled asset order or size invalid')
        previous, total = name, total + size
        compressed = name.endswith('.gz')
        logical = name[:-3] if compressed else name
        suffix = Path(logical).suffix
        mime = MIME.get(suffix)
        if logical == 'appversion' and not compressed:
            mime = 'application/octet-stream'  # Existing static-server MIME.
        if not mime or logical in public:
            raise ValueError('private, unsupported or shadow bundled asset')
        public.add(logical)
        result.append({'path': '/' + logical, 'file': name, 'mime': mime,
                       'gzip': compressed, 'size': size, 'sha256': digest})
        if len(result) > MAX_FILES:
            raise ValueError('too many bundled assets')
    if not REQUIRED.issubset('/' + name for name in public):
        raise ValueError('missing required bundled web entry')
    return result


def build_blob(directory):
    directory = Path(directory)
    verify_manifest(directory)
    manifest = (directory / FILENAME).read_bytes()
    entries = records(manifest)
    blob = MAGIC + struct.pack('>I', len(manifest)) + manifest
    for entry in entries:
        blob += (directory / entry['file']).read_bytes()
    extract(blob)  # Validate bytes, gzip, source version and aggregate bounds.
    return blob


def extract(firmware):
    if firmware.count(MAGIC) != 1:
        raise ValueError('firmware requires exactly one bundled web archive')
    start = firmware.index(MAGIC) + len(MAGIC)
    if len(firmware) - start < 4:
        raise ValueError('truncated bundled manifest length')
    size = struct.unpack_from('>I', firmware, start)[0]
    start += 4
    if not 0 < size <= MAX_MANIFEST or size > len(firmware) - start:
        raise ValueError('truncated or oversized bundled manifest')
    manifest = firmware[start:start + size]
    entries = records(manifest)
    position = start + size
    version = None
    for entry in entries:
        count = entry['size']
        if count > len(firmware) - position:
            raise ValueError('truncated bundled asset bytes')
        payload = firmware[position:position + count]
        position += count
        if hashlib.sha256(payload).hexdigest() != entry['sha256']:
            raise ValueError('bundled asset digest mismatch')
        expanded = payload
        if entry['gzip']:
            try:
                stream = zlib.decompressobj(16 + zlib.MAX_WBITS)
                expanded = stream.decompress(payload, MAX_EXPANDED + 1)
                if len(expanded) > MAX_EXPANDED or not stream.eof or stream.unused_data or stream.unconsumed_tail:
                    raise ValueError('bundled gzip exceeds bounds or has trailing content')
            except (OSError, EOFError, zlib.error) as error:
                raise ValueError('invalid bundled gzip') from error
        if entry['path'] == '/index.html':
            marker=b'<meta name="espsomfy-web-mode" content="bundled">'
            mode=WebMode()
            try:mode.feed(expanded.decode('utf-8'))
            except UnicodeError as error:raise ValueError('bundled HTML is not UTF-8') from error
            if expanded.count(marker)!=1 or mode.modes!=['bundled']:
                raise ValueError('firmware web UI must contain exactly one bundled-mode marker')
        if entry['path'] == '/appversion':
            try:
                version = payload.decode('ascii').strip()
            except UnicodeError as error:
                raise ValueError('invalid source application version') from error
            if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]{0,13}', version):
                raise ValueError('invalid source application version')
    return {'compatibility': compatibility(manifest), 'version': version,
            'assets': entries}


def generate(directory, output):
    blob = build_blob(directory)
    result = extract(blob)
    manifest_size = struct.unpack_from('>I', blob, len(MAGIC))[0]
    offset = len(MAGIC) + 4 + manifest_size
    chunks = ['#pragma once\nnamespace BundledWeb { namespace generated {\n',
              'static const uint8_t blob[] = {\n']
    for i in range(0, len(blob), 24):
        chunks.append(','.join(str(n) for n in blob[i:i + 24]) + ',\n')
    chunks.append('};\nstatic const Asset table[] = {\n')
    for entry in result['assets']:
        chunks.append('{%s,%s,blob+%d,%d,%s,%s},\n' % (
            json.dumps(entry['path']), json.dumps(entry['mime']), offset,
            entry['size'], 'true' if entry['gzip'] else 'false',
            json.dumps(entry['sha256'])))
        offset += entry['size']
    chunks.extend(['};\n',
                   f'static constexpr size_t manifestOffset={len(MAGIC) + 4}, manifestSize={manifest_size};\n',
                   'static const char version[]=' + json.dumps(result['version']) + ';\n',
                   '} }\n'])
    output = Path(output)
    if output.is_symlink():
        raise ValueError('generated bundle output must not be a symlink')
    data = ''.join(chunks).encode('ascii')
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_bytes() != data:
        with tempfile.NamedTemporaryFile(dir=output.parent,prefix=output.name+'.',suffix='.tmp',delete=False) as file:
            temporary=Path(file.name)
            try:file.write(data)
            except BaseException:
                temporary.unlink(missing_ok=True);raise
        try:temporary.replace(output)
        finally:temporary.unlink(missing_ok=True)
    return result

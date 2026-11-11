#!/usr/bin/env python3
"""Summarize compiler .su files without publishing host paths or device data."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def summarize(build_dir, limit=25):
    records = {}
    files = list(Path(build_dir).rglob('*.su'))
    for path in files:
        for line in path.read_text().splitlines():
            parts = line.rsplit('\t', 2)
            if len(parts) != 3:
                continue
            location, size, kind = parts
            match = re.fullmatch(r'(src/[^:]+):(\d+):(\d+):(.+)', location)
            if not match:
                continue  # Only repository source; omit SDK/cache/host paths.
            source, row, _, function = match.groups()
            if not size.isdecimal():
                raise ValueError('Invalid stack byte count')
            record = dict(source=source, line=int(row), function=function,
                          frameBytes=int(size), qualifier=kind)
            key = (source, row, function)
            if key not in records or record['frameBytes'] > records[key]['frameBytes']:
                records[key] = record
    ranked = sorted(records.values(), key=lambda r: (-r['frameBytes'], r['source'], r['line'], r['function']))
    selected = [r for r in ranked if any(name in r['function'] for name in (
        'SSDPClass::', 'ESPNetwork::networkEvent', 'ESPNetwork::discoveryLoop',
        'Transceiver::txTask', 'Transceiver::runJob', 'ProtoSocket::wsEvent',
        'void loop()', 'worker(void*)', 'StackAudit::'))]
    return dict(schema=1, units='bytes', measurement='compiler-per-function-frame',
                stackUsageFiles=len(files), uniqueRepositoryFunctions=len(records),
                largestFrames=ranked[:limit], selectedFrames=selected,
                limits=['Not a worst-case task stack bound or a runtime measurement.',
                        'Includes compiler inlining; prebuilt vendor libraries have no .su coverage.',
                        'Only src/ records are exported; duplicate template entries use the largest frame.',
                        'Dynamic qualifiers are retained; unexercised indirect calls and interrupts need separate analysis.'])


def binary_frames(disassembly):
    records = []
    address = name = ''
    for line in disassembly.splitlines():
        symbol = re.match(r'^([0-9a-f]+) <(.+)>:', line)
        if symbol:
            address, name = symbol.groups()
        entry = re.match(r'^\s*([0-9a-f]+):\s+[0-9a-f]+\s+entry\s+a1,\s*(0x[0-9a-f]+|[0-9]+)', line)
        # Decode only the instruction at the function symbol. A jump table can
        # look like an entry instruction when objdump disassembles its data.
        if entry and entry[1] == address:
            records.append(dict(function=name, frameBytes=int(entry[2], 0)))
    return sorted(records, key=lambda r: (-r['frameBytes'], r['function']))


def source_state():
    scope = ['src', 'platformio.ini', 'data-src', 'lib', 'boards', 'tools', 'minify.py', 'bundle_web.py', 'app_version.py', 'update_bootloader.py', 'archive_elf.py']
    names = subprocess.check_output(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard', '--'] + scope).split(b'\0')
    digest = hashlib.sha256()
    for name in sorted(set(names) - {b''}):
        digest.update(name + b'\0' + hashlib.sha256(Path(name.decode()).read_bytes()).digest())
    return dict(sourceCommit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
                sourceInputsSha256=digest.hexdigest(), sourceInputScope=scope)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build_dir', type=Path)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--objdump', required=True)
    parser.add_argument('--source-state', type=Path, required=True)
    args = parser.parse_args()
    report = summarize(args.build_dir)
    if not report['uniqueRepositoryFunctions']:
        parser.error('No repository stack-usage records found')
    state = json.loads(args.source_state.read_text())
    for field, length in [('sourceCommit', 40), ('sourceInputsSha256', 64)]:
        value = state.get(field, '')
        if not re.fullmatch(r'[0-9a-f]{'+str(length)+'}', value):
            parser.error('Invalid build source-state metadata')
        report[field] = value
    report['sourceInputScope'] = state['sourceInputScope']
    report['auditMode'] = state['auditMode']
    report['effectiveConfigSha256'] = state['effectiveConfigSha256']
    report['elfSha256'] = hashlib.file_digest(args.elf.open('rb'), 'sha256').hexdigest()
    report['compiler'] = subprocess.check_output([args.compiler, '--version'], text=True).splitlines()[0]
    report['environment'] = 'lilygo-t-embed-cc1101'
    prologues = binary_frames(subprocess.check_output([args.objdump, '-d', '-C', str(args.elf)], text=True))
    report['binaryFunctionPrologues'] = len(prologues)
    report['binaryLargestFrames'] = prologues[:25]
    report['limits'].append('Binary entries cover windowed-ABI prologues only; no complete call graph or interrupt bound is inferred.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f"Recorded {report['uniqueRepositoryFunctions']} repository functions; units=bytes; no runtime claim")


if __name__ == '__main__':
    main()

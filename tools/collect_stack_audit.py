#!/usr/bin/env python3
"""Export only allowlisted numeric stack samples from a private serial log."""
import argparse
import json
from pathlib import Path
import re

ROLES = ('loop', 'network', 'udp', 'web', 'radio', 'updater')
LINE = re.compile(r'STACK_AUDIT schema=1 role=(' + '|'.join(ROLES) + r') uptimeMs=(\d+) minimumUnusedBytes=(\d+) samples=(\d+) maxScanUs=(\d+)$')
BOUNDARY = re.compile(r'ESP-ROM:|rst:0x|Rebooting\.\.\.|Guru Meditation Error|Stack canary watchpoint|ELF file SHA256:')
IDENTITY = re.compile(r'LCD running slot=app[01] elfSHA256=([0-9a-f]{64})(?:$|\s)')
ANSI = re.compile(r'\x1b\[[0-9;]*m')


def parse(lines):
    records, previous, rejected = [], {}, 0
    captured_elf = None
    boot_seen = False
    for line in lines:
        clean = ANSI.sub('', line.rstrip('\r\n'))
        identity = IDENTITY.search(clean)
        if BOUNDARY.search(clean):
            if records:
                raise ValueError('Boot/crash boundary after samples; split capture into separate runs')
            captured_elf = None
            boot_seen = bool(re.search(r'ESP-ROM:|rst:0x', clean))
        if identity:
            # Tasks can log their first sample before setup prints the ELF.
            # Bind that first identity only within an observed uninterrupted boot.
            if records and (captured_elf is not None or not boot_seen):
                raise ValueError('Image identity boundary after samples; split capture into separate runs')
            captured_elf = identity[1]
        match = LINE.search(clean)
        if not match:
            rejected += int('STACK_AUDIT' in clean)
            continue
        role, uptime, unused, count, overhead = match.groups()
        uptime, unused, count, overhead = map(int, (uptime, unused, count, overhead))
        if not (uptime < 2**64 and unused <= 1048576 and 0 < count < 2**32 and overhead <= 10000000):
            rejected += 1
            continue
        if role in previous:
            last = previous[role]
            if uptime < last['uptimeMs'] or count < last['samples'] or unused > last['minimumUnusedBytes']:
                raise ValueError('Task history restarted or changed; split evidence into separate runs')
        record = dict(role=role, uptimeMs=uptime, minimumUnusedBytes=unused, samples=count, maxScanUs=overhead)
        records.append(record)
        previous[role] = record
    if not records:
        raise ValueError('No valid stack-audit samples; no runtime result can be claimed')
    return dict(schema=1, units='bytes', measurement='rtos-stack-fill-high-water-mark',
                samples=records, capturedElfSha256=captured_elf, rolesObserved=sorted(previous),
                rolesUnobserved=sorted(set(ROLES)-set(previous)), rejectedAuditLines=rejected,
                limits=['Fill-pattern high-water marks are observations, not a worst-case stack bound.',
                        'Samples include instrumentation effects; maxScanUs excludes UART/logging time.',
                        'Activity-dependent self-sampling: at most once per 5 seconds; output at most once per 30 seconds per role.',
                        'A role can retain its minimum across task recreation; split captures at reboot.',
                        'No RF, updater, network-reconnect or full-load coverage is implied by an idle sample.',
                        'Continuous capture is required: missing boot/identity markers can conceal a reboot.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('private_log', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--source', required=True, help='Exact instrumented source commit, 40 hexadecimal characters')
    parser.add_argument('--elf-sha256', required=True)
    parser.add_argument('--board', choices=('t-embed-cc1101', 't-embed-cc1101-plus'), required=True)
    parser.add_argument('--workload', choices=('idle', 'read-load', 'network-reconnect', 'updater'), required=True)
    args = parser.parse_args()
    if not re.fullmatch(r'[0-9a-f]{40}', args.source) or not re.fullmatch(r'[0-9a-f]{64}', args.elf_sha256):
        parser.error('Use full source/ELF hashes')
    try:
        with args.private_log.open(errors='replace') as source:
            report = parse(source)
    except ValueError as error:
        parser.error(str(error))
    if report['capturedElfSha256'] and report['capturedElfSha256'] != args.elf_sha256:
        parser.error('Captured running ELF differs from the declared ELF')
    report.update(declaredSourceCommit=args.source, declaredElfSha256=args.elf_sha256,
                  capturedElfMatchesDeclared=report['capturedElfSha256'] == args.elf_sha256,
                  boardModel=args.board, workload=args.workload)
    report['limits'].append('CLI source/ELF metadata is declared; only a captured boot fingerprint can be compared with the declared ELF.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print(f"Exported {len(report['samples'])} sanitized samples; unobserved roles: {', '.join(report['rolesUnobserved']) or 'none'}")


if __name__ == '__main__':
    main()

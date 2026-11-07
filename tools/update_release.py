#!/usr/bin/env python3
"""Prepare exact-board firmware-only GitHub assets offline; never publish or flash."""
import argparse
import json
import re
from pathlib import Path
import shutil

import lilygo_manifest as build
from update_assets import verify_manifest
from update_profiles import load_profile
import update_package
from update_identity import (BOARD, LAYOUT, stable_tag, MANIFEST_ASSET,
                             FIRMWARE_ASSET, identity, tags_at_head)


def read_app_compat(source):
    return verify_manifest(source / 'data')


def release_manifest(package, source, version, environment=BOARD):
    if not stable_tag(version):
        raise ValueError('release version must be a stable vMAJOR.MINOR.PATCH tag')
    profile = load_profile(source, environment)
    if not profile:
        raise ValueError('environment has no registered verified-update profile')
    if (package / 'update-build.json').exists():
        evidence = json.loads((package / 'update-build.json').read_text())
        update_package.validate(evidence['artifacts'], package, profile)
        manifest = {'source': evidence['source']}
    elif environment == BOARD:
        # Preserve the independently checked legacy Plus provisioning package.
        manifest = build.validate(json.loads((package / 'flash-manifest.json').read_text()), package)
        effective = manifest['effective_build']
        update_package.feature_configuration(effective['resolved'], effective['compiler'], profile)
    else:
        raise ValueError('generic release requires exact-profile update-build.json evidence')
    provenance = json.loads((package / 'update-source.json').read_text())
    built_source = provenance['source']
    current_source = build.source_record(source)
    if built_source['dirty'] or current_source['dirty'] or manifest['source']['dirty']:
        raise ValueError('release requires a clean source tree before and after compilation')
    if built_source != current_source or built_source != manifest['source']:
        raise ValueError('source changed since compilation; rebuild the reviewed tagged commit')
    app_compat = read_app_compat(source)
    expected_identity = identity(built_source, app_compat, tags_at_head(source), profile)
    if version != expected_identity['version'] or provenance['identity'] != expected_identity:
        raise ValueError('release tag or build identity mismatch; rebuild from the release tag')
    firmware = (package / 'firmware.bin').read_bytes()
    # The compiled marker is part of the image covered by its ESP checksum/SHA.
    # Require the delimiter and terminator to defeat prefix/version substitutions.
    if len(firmware) < 1024:
        raise ValueError('firmware is below the supported runtime size')
    marker = expected_identity['marker'].encode('ascii') + b'\0'
    if firmware.count(marker) != 1:
        raise ValueError('firmware does not contain exactly one matching compiled build identity')
    markers = re.findall(rb'ESPSOMFY_UPDATE_ID:1\|[a-z0-9-]+\|[A-Za-z0-9_]+\|[0-9a-f]{40}\|[A-Za-z0-9.-]+\|sha256:[0-9a-f]{64}\x00', firmware)
    if markers != [marker]:
        raise ValueError('firmware contains conflicting update identities')
    bootloader = (package / 'bootloader.bin').read_bytes()
    boot_marker = ('ESPSOMFY_BOOT_ID:' + str(profile['bootloader_address']) + ':' +
                   str(len(bootloader)) + ':' + build.digest(bootloader)).encode('ascii') + b'\0'
    boot_markers = re.findall(rb'ESPSOMFY_BOOT_ID:[0-9]+:[0-9]+:[0-9a-f]{64}\x00', firmware)
    if firmware.count(b'ESPSOMFY_BOOT_ID:') != 1 or boot_markers != [boot_marker]:
        raise ValueError('firmware compiled bootloader identity differs from actual package bootloader')
    if len(firmware) > profile['app_bytes']:
        raise ValueError('firmware exceeds exact-profile OTA slot')
    return {'schema': 1, 'board': profile['board'], 'layout': profile['layout'],
            'flash_bytes': profile['flash_bytes'], 'psram_bytes': profile['psram_bytes'],
            'bootloader_sha256': build.digest(bootloader),
            'app_compat': app_compat, 'version': version,
            'source_sha': built_source['revision'], 'asset': 'firmware-' + profile['board'] + '.bin',
            'size': len(firmware), 'sha256': build.digest(firmware), 'channel': 'stable'}


def write_release(package, source, version, output, environment=BOARD):
    # Never overwrite a prior package or caller-owned directory.
    if output.exists() or output.is_symlink():
        raise FileExistsError('output must be a new directory')
    manifest = release_manifest(package, source, version, environment)
    output.mkdir(parents=True, exist_ok=False)
    shutil.copyfile(package / 'firmware.bin', output / manifest['asset'])
    (output / ('update-' + manifest['board'] + '.json')).write_text(json.dumps(manifest, sort_keys=True, indent=2) + '\n')
    # Keep full build evidence separately; publish only the two exact OTA assets.
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path, help='validated tools/build-update.sh or legacy Plus artifacts directory')
    parser.add_argument('--source', required=True, type=Path, help='clean tagged source used to compile')
    parser.add_argument('--version', required=True, help='reviewed stable vMAJOR.MINOR.PATCH tag at HEAD')
    parser.add_argument('--output', required=True, type=Path, help='new directory for the two OTA assets')
    parser.add_argument('--environment', default=BOARD, help='exact registered build environment (default: Plus)')
    args = parser.parse_args()
    try:
        manifest = write_release(args.package, args.source, args.version, args.output, args.environment)
    except (ValueError, KeyError, OSError) as error:
        parser.exit(1, f'Release rejected: {error}\n')
    print(f"Prepared {manifest['version']} for {manifest['board']}: {args.output}")


if __name__ == '__main__':
    main()

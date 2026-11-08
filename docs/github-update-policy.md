# Offline GitHub update policy foundation

This slice reuses the reviewed algorithms from issue #12 commit `baf01473f28a67e689b0a2565c78d83903a784b3`, reconciled with current main. It supplies compatibility/build/release policy only. It does not enable `ESPSOMFY_GITHUB_UPDATES`, add a worker, call GitHub, download, install, reboot, flash or access hardware. Future web and LCD adapters must reuse the single reviewed `GitUpdateClass` service, not create another installer.

## Trust and exact compatibility

Sid approved trust in `SidShaytay/ESPSomfy-RTS` GitHub releases over verified HTTPS and GitHub API asset SHA256 digests. Those digests are not independent publisher signatures. The later transport must enforce TLS certificates/hostname/time, repository identity, published/non-draft/non-prerelease latest stable metadata, bounded JSON and body sizes, and digest verification at every transfer. `allowedUrl()` only admits exact HTTPS authorities `api.github.com`, `github.com` and `release-assets.githubusercontent.com`; every redirect needs the same check. A host allowlist alone does not establish release authenticity.

Schema **2** deliberately rejects old schema 1. There is exactly one supported profile, `lilygo-t-embed-cc1101`, for the explicit ordered hardware list `lilygo-t-embed-cc1101`, `lilygo-t-embed-cc1101-plus`. These physical variants share the reviewed used wiring, flash/PSRAM and partition layout. Plus-only and suffix profile IDs, generic boards and unknown memberships are rejected; none is an alias. Historical installation migration remains a separate issue #12 dependency.

The release contract requires the exact profile, both hardware members, `default_8MB` layout, 16 MiB flash, 8 MiB PSRAM, canonical slot size, and features mask **7** (bit 0 display, bit 1 interactive UI, bit 2 local commands). It binds source SHA, stable `vMAJOR.MINOR.PATCH`, exact firmware filename/size/SHA256, bootloader SHA256 and `sha256:` compatibility digest of the exact packaged web files. Version components fit unsigned 32-bit integers; leading zeros, prereleases, metadata suffixes, downgrades and same-version updates are refused. Manifest strings and numeric values must be parsed without truncation before calling policy.

`GitUpdatePolicy::check()` returns a typed compatibility reason; `reason()` provides safe constant text. The existing worker-compatible `validate()` wrapper delegates to that same policy. `newer()` requires two stable versions, so a development build cannot silently become a release candidate. `hasIdentity()` requires one complete matching, NUL-terminated schema-2 marker and rejects duplicate/conflicting markers. This does not replace ESP image framing/checksum/chip validation or digest authentication.

## Deterministic assets and build identity

Only the canonical shared environment uses deterministic gzip (timestamp zero, no filename), the existing tested UTF-8 bundling/fallback behavior, copied PNGs, and no optional Terser/optipng transformation. The generic build path retains its existing behavior. Tests compare the complete decompressed JS/CSS bundles with the existing source concatenation; no new regex minifier is enabled.

`data/update-assets.txt` records sorted safe paths, exact sizes and SHA256 of every packaged file except itself, bounded to 128 files, 16 KiB manifest and the filesystem payload limit. Its SHA256 is the application compatibility identity. Changed, missing, extra, unsafe or symlinked files fail verification. The later installed-assets verifier still needs integration; a generated manifest is not evidence that a controller has matching LittleFS.

`minify.py` runs before `app_version.py`. The shared build derives source provenance, source tag and assets identity before compilation, emits identity defines and `update-source.json`, and retains a named identity symbol through the pinned toolchain's linker garbage collection. Generic firmware keeps the existing `FW_VERSION` behavior and receives no supported updater identity. The source hash inventory includes `update-profiles.json`. The current build packager remains authoritative for board/compiler/library/image/partition checks and now preserves `update-source.json` alongside its evidence.

Dirty, untagged, prerelease-tagged or ambiguously stable-tagged sources compile with explicit `dev-*` identity. This is still the full-feature shared firmware; it cannot be packaged as a stable update. No tag is invented or published by these tools.

## Offline release preparation and remaining dependencies

Run `python3 tools/update_release.py <artifacts> --source <clean-tagged-source> --version vX.Y.Z --output <new-directory>` only against an already reviewed local tagged build. It never creates tags or publishes assets. It checks the current packager contract, clean and unchanged source before/after compilation, exact tag, actual compiler identity defines/raw override flags, packaged assets, compiled image identity and bootloader marker. Output must be a new directory.

The bootloader marker generator/installed attestation, runtime asset verification, CA/time bootstrap, allocator/worker, admission/quiescence, upload exclusion, boot health/rollback and candidate-bound confirmation are **not integrated by this slice**. Consequently current development output is not a deployable updater release; missing bootloader identity makes release preparation fail closed. Runtime check/install must remain unavailable when installed bootloader or assets attestation is absent. Supplying those installed prerequisites requires separate explicitly authorized inspection/provisioning; this document authorizes no such stage.

Future confirmation must bind the displayed source/version/image digest/manifest digest and candidate generation. Refreshing latest after confirmation must return Changed if any bound identity changes. A refreshed release must never be installed under confirmation of another candidate.

## Offline verification

- `python3 -m unittest discover -s tools -p 'test_update_*.py'`: profile, assets, pre-build identity and release provenance/negative tests.
- `python3 -m unittest discover -s tools -p 'test_lilygo_manifest.py'`: existing canonical package guard regressions.
- `pio test -e native`: policy/version/URL/identity tests alongside current firmware domain tests.
- `node --test 'test/js/*.test.js'`, board-profile and UI-adapter host harnesses retain current web/auth/board behavior.
- Serial build-only shared and generic firmware checks; inspect the shared image for exactly one development marker and generic image for none. Package the shared firmware/filesystem together using `tools/build-lilygo.sh`.

All tests use fake/local boundaries. No controller, HA, RF, motor commands, publication or physical recovery qualification is included.

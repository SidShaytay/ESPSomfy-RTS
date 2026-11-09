# Offline updater prerequisite attestation

This slice extracts and narrows issue #12's committed bootloader and web-asset checks from `baf01473f28a67e689b0a2565c78d83903a784b3`. The current shared CC1101/Plus policy is schema 3 with bundled web assets; the original filesystem implementation is retained only in Git history. It adds no worker, network request, startup invocation, installer, flashing path or bootstrap operation. The checks below are callable read-only prerequisites for the later single updater service.

## Typed results and ownership

`gitUpdateBootloaderStatus()` and `gitUpdateAssetsStatus()` return `UpdateAttestation`: verified, unsupported, missing, invalid, bounds, readError, hashError, mismatch, encrypted, directoryError or unexpectedAsset. Every result except verified makes the prerequisite unavailable. Existing boolean helpers delegate to the same checks; they are not separate implementations. Generic firmware returns unsupported without reading flash or filesystem contents.

Missing prerequisites never authorize provisioning. Installed bootloader/assets state remains unknown until an explicitly authorized live inspection stage. No controller was inspected while developing this slice. The future worker must report unavailable, not attempt automatic repair, when these checks fail.

## Bootloader build binding

`pre:update_bootloader.py` configures only the canonical shared environment. The build uses the pinned `esp32s3/qio_opi` SDK configuration, requiring exactly one enabled rollback define and the shared offset zero, with conflicting undefines rejected. The generator validates the actual `bootloader.bin` ESP image using the current canonical image validator, bounds it below the partition table, and hashes its actual bytes.

SCons dependencies force bootloader conversion before generation and compilation of `GitUpdateBootloader.cpp`. The generated header stays in the isolated build directory. A named C identity symbol, rooted explicitly by the linker, retains `ESPSOMFY_BOOT_ID:<address>:<length>:<sha256>` inside the application image. The existing offline release packager already requires that marker to match the packaged bootloader. No SDK or board configuration is rewritten.

The runtime hook refuses encrypted flash configurations, then streams raw reads through an aligned 512-byte buffer and the pinned SDK SHA256 implementation. Read, hash and mismatch errors are distinct typed refusals. The result is cached for the boot because this updater never writes the bootloader. Test boundaries inject encryption, read/start/update/finish failures and changed bytes; generic checks require no SDK flash implementation.

## Immutable served web assets

Sid selected bundling the public web UI inside the shared application image. The canonical asset check hashes the compiled manifest against `ESPSOMFY_APP_COMPAT`, then hashes each immutable table entry using the same read-only `BundledWeb` implementation used by the HTTP routes. It never opens private configuration, enumerates LittleFS or reads a filesystem web version. Generic boards return unsupported here and retain their existing filesystem serving behavior.

The generator accepts at most 128 exact root public assets and a 16 KiB canonical manifest. Each packed file is bounded by the existing 0x180000-byte asset limit; gzip expansion is bounded at 8 MiB and must terminate with no appended stream. Required HTML, JavaScript, CSS and source-version entries must exist. Private paths, aliases/shadows, unknown assets, malformed gzip, missing files and mismatched size/digest fail the build. The HTML must contain exactly one bundled-mode marker before the compatibility digest is calculated.

The runtime hashes in 1 KiB chunks, checking cooperative cancellation before I/O and between chunks and releasing hash state on all exits. HTTP uses immutable flash pointers with the existing MIME, gzip, ETag and no-cache behavior, and never falls back to filesystem web files. Registration failure remains observable through route readiness; this is not complete postboot-health acceptance.

## Offline tests

- `python3 -m unittest discover -s tools -p 'test_update_*.py'`: existing release/profile/assets tests plus bootloader generation, rollback/offset and corrupt/oversized image rejection.
- `bash tools/test-update-bootloader.sh`: production read/hash/cache code with fake flash/hash/encryption boundaries and generic unsupported checks.
- `bash tools/test-update-assets.sh`: production bundled hashing and HTTP routes with an independent host SHA256 oracle, hash/cancellation failures, immutable/private-path checks, registration failures, generated manifest validation and compiled-versus-filesystem version tests.
- Full native suite, existing canonical package/board/auth harnesses, and serial shared/generic firmware builds retain current behavior. The fresh shared build log establishes bootloader conversion → header generation → attestation compilation ordering; the built image contains exactly one matching bootloader marker.

All tests are local/fake or build-only. Successful compilation and matching packaged markers establish no installed bootloader state, power-loss rollback behavior, physical flash access, radio behavior or controller recovery acceptance.

# Offline updater prerequisite attestation

This slice extracts and narrows issue #12's committed bootloader and web-asset checks from `baf01473f28a67e689b0a2565c78d83903a784b3`. It builds on the schema-2 shared CC1101/Plus policy. It adds no worker, network request, startup invocation, installer, flashing path or bootstrap operation. The checks below are callable read-only prerequisites for the later single updater service.

## Typed results and ownership

`gitUpdateBootloaderStatus()` and `gitUpdateAssetsStatus()` return `UpdateAttestation`: verified, unsupported, missing, invalid, bounds, readError, hashError, mismatch, encrypted, directoryError or unexpectedAsset. Every result except verified makes the prerequisite unavailable. Existing boolean helpers delegate to the same checks; they are not separate implementations. Generic firmware returns unsupported without reading flash or filesystem contents.

Missing prerequisites never authorize provisioning. Installed bootloader/assets state remains unknown until an explicitly authorized live inspection stage. No controller was inspected while developing this slice. The future worker must report unavailable, not attempt automatic repair, when these checks fail.

## Bootloader build binding

`pre:update_bootloader.py` configures only the canonical shared environment. The build uses the pinned `esp32s3/qio_opi` SDK configuration, requiring exactly one enabled rollback define and the shared offset zero, with conflicting undefines rejected. The generator validates the actual `bootloader.bin` ESP image using the current canonical image validator, bounds it below the partition table, and hashes its actual bytes.

SCons dependencies force bootloader conversion before generation and compilation of `GitUpdateBootloader.cpp`. The generated header stays in the isolated build directory. A named C identity symbol, rooted explicitly by the linker, retains `ESPSOMFY_BOOT_ID:<address>:<length>:<sha256>` inside the application image. The existing offline release packager already requires that marker to match the packaged bootloader. No SDK or board configuration is rewritten.

The runtime hook refuses encrypted flash configurations, then streams raw reads through an aligned 512-byte buffer and the pinned SDK SHA256 implementation. Read, hash and mismatch errors are distinct typed refusals. The result is cached for the boot because this updater never writes the bootloader. Test boundaries inject encryption, read/start/update/finish failures and changed bytes; generic checks require no SDK flash implementation.

## Exact served web namespace

The asset hook authenticates `/update-assets.txt` against the compiled schema-2 application compatibility digest, then validates every listed file's exact size and digest. Bounds remain 128 listed files, 16 KiB manifest, 127-character absolute safe paths, 224-byte line buffer, and aggregate file payload no larger than the canonical 0x180000-byte filesystem partition. The file hashing buffer is 1 KiB; no complete filesystem or manifest is staged in RAM. Empty files are supported. Malformed, duplicate, unsorted, truncated, overflowing, missing, mismatched and unreadable inputs fail closed. Directories cannot satisfy listed file records.

Root directory metadata is enumerated through `opendir(LittleFS.mountpoint())`, `readdir` with a cleared errno, and checked `closedir`. The pinned Arduino `File::openNextFile()` collapses iterator errors into end-of-directory, which cannot meet the required error contract. POSIX metadata enumeration permits explicit open/iterator/close errors, empty/omitted results, duplicate entries and excessive names/counts to be refused. At most 256 root entries and 127-character entry names are accepted.

The existing `HttpAccess.h::isWebAsset()` defines the served root namespace. Every unlisted root entry with a served-shaped name, or a `.gz` counterpart of one, is refused regardless of whether its reported type is a file, directory or unknown. This catches plain/gzip shadows and unrelated extra served files. It also catches directories such as `unexpected.js`, since the pinned static handler can append its default `index.html` after an unsuccessful file lookup. Listed root entries must be regular files and all expected served root entries must appear exactly once in metadata.

Ordinary directories and nonservable private configuration/temporary names are ignored. Directory contents are not enumerated; private configuration/counter files are never opened or hashed. Listed nested packaged files still receive their required digest checks, while the current root-only web access policy rejects direct nested URLs. No web serving policy changes are made.

To avoid storing a large name table, root membership is resolved by rescanning the bounded manifest, with watchdog resets. The manifest is hashed again after verification to detect changes during the check. This remains a point-in-time check: the future updater's exclusive ownership/quiescence must prevent filesystem uploads or other relevant writes throughout check/install/health operations. No atomic whole-filesystem snapshot or physical latency qualification is claimed.

## Offline tests

- `python3 -m unittest discover -s tools -p 'test_update_*.py'`: existing release/profile/assets tests plus bootloader generation, rollback/offset and corrupt/oversized image rejection.
- `bash tools/test-update-bootloader.sh`: production read/hash/cache code with fake flash/hash/encryption boundaries and generic unsupported checks.
- `bash tools/test-update-assets.sh`: production parser, hashing and root enumeration with local fixture files, an independent host SHA256 oracle, injected metadata/hash/read failures, unexpected served files/directories, private-file open traps and authenticated malformed manifests.
- Full native suite, existing canonical package/board/auth harnesses, and serial shared/generic firmware builds retain current behavior. The fresh shared build log establishes bootloader conversion → header generation → attestation compilation ordering; the built image contains exactly one matching bootloader marker.

All tests are local/fake or build-only. Successful compilation and matching packaged markers establish no installed bootloader state, power-loss rollback behavior, physical flash access, radio behavior or controller recovery acceptance.

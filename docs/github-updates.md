# Verified firmware updates for the LILYGO Plus

This fork adds an exact-board GitHub update path for the T-Embed CC1101 Plus. Development and offline verification do not authorize accessing or changing the in-service controller. Installation, release publication, and live acceptance remain pending the separately approved service window described below.

## Prepare reviewable release assets offline

Use the pinned user-owned tooling from `tools/setup-lilygo.sh`. Build from the reviewed stable `vMAJOR.MINOR.PATCH` tag in a clean worktree, then package the resulting artifacts:

```bash
bash tools/build-lilygo.sh
python3 tools/update_release.py build/lilygo/artifacts \
  --source . --version <reviewed-tag> --output build/ota-release
```

Replace `<reviewed-tag>` with the exact tag at that worktree's `HEAD`. The output directory must not exist. Packaging validates the complete Plus artifact package, partition table, image checksums and appended SHA256, prebuild/postbuild/current source provenance, exact tag, and compiled board/layout/revision/version/application marker before producing these two assets:

- `firmware-lilygo-t-embed-cc1101-plus.bin`
- `update-lilygo-t-embed-cc1101-plus.json`

The packager never publishes, contacts a controller, or opens a serial port. Keep the full artifact package, compiler metadata, logs, ELF symbols and reviewed source alongside the release evidence. Build provenance identifies source inputs; it does not claim bit-for-bit reproducible compiler output. Build scripts that timestamp or gzip assets may differ across invocations, and the full filesystem is not published by this firmware-only updater.

The inherited application version currently reads `4.0.1`, despite the upstream source baseline being cjkas `v4.0.2`. The release version and exact source revision are therefore separate identity fields. Dirty or untagged builds carry a `dev-<revision>` identity and cannot be repackaged as stable releases. Keeping this exact application compatibility version is a prerequisite for firmware-only updates; changing the web assets requires a separate compatibility/migration design.

## Understand the trust boundary

The only update repository is `SidShaytay/ESPSomfy-RTS`, stable releases only. There is no fallback to cjkas, rstrouse, another fork, or a generic ESP32-S3 binary. GitHub repository control, GitHub's verified HTTPS endpoints, the installed CA trust bundle and the device's time are the trust roots. Manifest SHA256 binds downloaded bytes to that authenticated release metadata; it is **not an independent publisher signature** and does not protect against compromise of the trusted GitHub repository or account.

Protect release permissions and use immutable published releases. Review the source tag, generated manifest, digest and full build evidence before publishing both assets on that same release. No release has been published by this implementation task. Do not overwrite an existing release asset to silently change its contents. A change of repository or any future independent signing key needs a reviewed firmware change; certificate roots follow the pinned framework's CA bundle, with no insecure fallback.

The manifest binds schema, stable channel, version, exact source SHA, Plus board profile, `default_8MB` layout, 16 MiB flash, 8 MiB PSRAM, exact installed web application compatibility version, exact firmware asset name, byte count and SHA256. Chip family, flash size or filename alone never establishes exact-board compatibility. See [the memory table](board-memory.md) for other supported boards; they do not inherit this Plus-only updater.

## Preserve controller state and recovery

The update path stages and verifies a firmware image before making a new application bootable. It writes the inactive application slot only. It does not migrate partitions, replace LittleFS, restore backups, alter radio identities/calibration, or intentionally advance/reset rolling counters. Ordinary in-service operation can still advance counters while a background download is running.

The installed Plus uses app slots at `0x10000` and `0x340000`, each `0x330000` bytes; NVS remains `0x9000`/`0x5000`, and shared LittleFS remains `0x670000`/`0x180000`. The physical flash is 16 MiB with the upper 8 MiB unallocated. Two application slots do not make a shared-filesystem replacement atomic. Keep filesystem upload, USB provisioning and any layout migration separate.

ESP-IDF application rollback support must be enabled in the installed bootloader, and a new application must remain pending until the postboot health policy confirms it. The build hashes the actual generated rollback-enabled Plus bootloader; runtime reads and hashes the installed bootloader bytes and refuses GitHub updates or health confirmation if they differ. This strict check also refuses an otherwise functional older/custom bootloader until its compatibility is explicitly reviewed; it never rewrites the bootloader. An application-only update cannot enable rollback in an old bootloader. A healthy networking loop alone does not prove RF/motor behavior, persistent counters, or complete recovery. Existing recovery evidence in issue #9 predates this feature and cannot substitute for the failure tests below.

## Approve a specific service window

After independent offline review and successful host/firmware builds, prepare a dated service-window request with these exact artifacts and recovery details. Sid's authorization must precede controller access or any write/test operation.

1. Record the reviewed source revision, exact stable release tag, manifest and firmware SHA256, current installed identity, partition evidence, bootloader rollback capability and chosen test sequence. Publish reviewed assets only with publication authorization.
2. Pause motion automations and ensure shutters are stationary for the approved period. Capture fresh complete flash and portable configuration/state snapshots privately, including current linked-remote/shade counters. Record any subsequent commands; stale snapshots must not restore stale counters.
3. Use an identified spare exact Plus board for failure injection where possible, without simultaneously running duplicate radio identities. Test valid and expired/untrusted/wrong-host certificates, invalid time, redirect rejection/validation, truncated/oversized/corrupt/wrong-board/wrong-layout/incompatible-app assets, slow/unreachable servers, connection loss, repeated checks/downloads and cancel/retry.
4. Measure internal minimum free heap, largest free internal block, PSRAM staging/handshake peaks and worker stack headroom under fragmented memory. Observe radio scheduling and web responsiveness during background work. Confirm installation's flash-write pause is bounded and no unexpected motor command occurs.
5. Test interruption during staging and inactive-slot writing, first-boot health failure, and automatic rollback. Verify NVS/LittleFS/state/counter continuity before and after success and rollback. Test recovery to the known-good exact-board application without restoring stale identities/counters.
6. Load the approved candidate on the in-service controller only after the agreed prerequisite checks pass. Observe the same state continuity and responsiveness, then perform only the explicitly authorized RF/motor tests. Keep HA installation controls disabled until the complete supported path has passed.

The review must record measured results and unresolved failures rather than treating successful compilation as hardware acceptance. No service-window step has been performed by this development task.

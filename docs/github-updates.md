# Verified GitHub firmware updates

This fork adds device-side GitHub updates to explicit release profiles with enough PSRAM, matching packaged web assets, an attested rollback-enabled bootloader and a compatible dual-slot layout. Development and offline verification do not authorize accessing or changing the in-service controller. Release publication and live acceptance remain pending a separately approved service window.

## Choose the exact board profile

The single profile contract is [`update-profiles.json`](../update-profiles.json). It enables the Plus, standard T-Embed CC1101, PSRAM-equipped S3 release, official C5 N8R8 release, and the explicit `esp32p4-psram`/`esp32p4es-psram` targets. Every profile requires at least 8 MiB working PSRAM; actual flash capacity, partition addresses, compiled board identity, web assets, bootloader digest and image size are checked separately. [The complete memory table](board-memory.md) distinguishes these targets from older no-PSRAM C5, generic no-PSRAM S3, and single-slot debug builds.

A profile's presence does not establish hardware acceptance. Standard LILYGO and the additional memory variants have not been tested on hardware by this task. Existing `esp32p4`/`esp32p4es` profiles retain their smaller slots and manual update behavior: a TLS-enabled P4 image exceeded their 1.5 MiB slots. New explicit release profiles provide enough space without silently changing an existing target: `esp32p4-psram` uses `default_8MB` on 16 MiB flash; `esp32p4es-psram` uses a dedicated 4 MiB table with two `0x1C0000`-byte application slots and 384 KiB shared filesystem. Each final image must still pass its actual slot/headroom check. Unconfigured memory variants require a reviewed exact profile; a larger PSRAM chip never selects another board's artifact.

## Prepare reviewable release assets offline

Use the pinned user-owned tooling from `tools/setup-lilygo.sh`. Build from the reviewed stable `vMAJOR.MINOR.PATCH` tag in a clean worktree, then package the resulting artifacts. For example, for the Plus:

```bash
bash tools/build-update.sh lilygo-t-embed-cc1101-plus build/update-review
python3 tools/update_release.py build/update-review/artifacts \
  --source . --environment lilygo-t-embed-cc1101-plus \
  --version <reviewed-tag> --output build/ota-release
```

Replace `<reviewed-tag>` with the exact tag at that worktree's `HEAD`. Select the matching environment for another supported profile. The output directory must not exist. Packaging validates the profile, actual partition table, compiler configuration/macros, image checksums and appended SHA256, exact packaged web-asset fingerprint, prebuild/postbuild/current source provenance, exact tag and compiled board/layout/revision/release/application marker.

The release directory contains exactly `firmware-<environment>.bin` and `update-<environment>.json`. The build package additionally retains `update-build.json`, bootloader/partition/filesystem artifacts, compiler metadata and symbols for independent review and approved provisioning. Build scripts and the packager never publish, contact a controller or open a serial port.

Keep the full artifact package, logs, ELF symbols and reviewed source with the release evidence. Source provenance identifies the build inputs; it does not establish bit-for-bit reproducible compiler output. Deterministic packaged web assets and their manifest are tested separately. The shared filesystem is not replaced by the GitHub firmware-only path.

The inherited application version currently reads `4.0.1`, despite the cjkas source baseline being `v4.0.2`. Release identity therefore uses its own exact tag and source SHA. Dirty or untagged builds carry `dev-<revision>` and cannot be repackaged as stable releases or install a subsequent stable update. The first approved updater deployment must use a clean reviewed stable-tag build.

The compatibility field is a `sha256:` fingerprint of the exact deterministic packaged web assets. Keeping an application version label unchanged is insufficient: changing any covered HTML, JavaScript, CSS, image or application-version file requires an explicitly approved coherent web-assets/firmware provisioning step. Firmware-only releases must preserve the exact installed asset fingerprint.

## Understand the trust boundary

The only update repository is `SidShaytay/ESPSomfy-RTS`, stable releases only. There is no fallback to another fork or a generic chip-family binary. GitHub repository control, verified HTTPS endpoints, the installed CA trust bundle and device time are the trust roots. The GitHub API's authenticated asset digests bind both downloaded manifest and firmware bytes to the release; missing digests fail closed. This is **not an independent publisher signature** and does not protect against compromise of the trusted repository or GitHub account.

Protect release permissions and use immutable published releases. Review the source tag, generated manifest, digest and build evidence before publishing both exact assets on the same release. No release has been published by this task. Do not overwrite an existing asset to silently change its contents. Repository changes or a future independent signing-key policy require reviewed firmware changes. Certificate roots follow the pinned framework's bundle; there is no insecure fallback. The configured NTP service remains the existing network trust dependency for clock bootstrap, and HTTPS refuses a clock earlier than the compiled source floor.

The manifest binds schema, stable channel, version, source SHA, exact board profile, layout, flash/PSRAM requirements, exact web-asset fingerprint, firmware asset name/size/SHA256 and required bootloader SHA256. The packager also requires the image's compiled bootloader address/size/digest marker to match the actual bootloader artifact, preventing a stale artifact from mislabelling a candidate. A candidate requiring another bootloader is refused before staging. A framework/bootloader change needs its own approved provisioning and recovery plan; firmware-only GitHub updates cannot migrate it.

## Preserve controller state and recovery

The worker downloads and verifies a complete image in PSRAM before preparing an inactive application slot. It never emits worker-task socket events or pumps the web loop. Authenticated HTTP status polling reports progress and sampled internal free/largest-block values, PSRAM/TLS allocation peaks and worker stack headroom. Actual handshake and radio/web timing still require live measurement.

Before release checks or installation, the worker requires at least 48 KiB free internal RAM and a 16 KiB largest internal block after its stack is allocated. These conservative admission limits allow a temporary resource error and retry; they do not guarantee handshake headroom under concurrent load. The separate postboot health check keeps its 24 KiB internal reserve, and actual peaks still require service-window measurement.

Before installation, the main loop finishes normal servicing and refuses pending transmissions, received frames, delayed shade actions, GPIO releases or queued requests. It freezes admission, pauses reception, and saves a preallocated, length-checked configuration snapshot with verified readback and atomic replacement. A failed preparation restores admission and reception. Installation writes the inactive application only; every terminal result after successful preparation reboots into the selected image or retained old slot.

Verified-update profiles refuse the web filesystem upload route (`/updateApplication`). Filesystem changes require coherent firmware/assets provisioning during an approved service window; replacing mounted shared LittleFS cannot preserve state or support application-slot rollback by itself. Manual firmware uploads remain available, with single-request ownership, activation only after the complete authenticated upload, and bounded loop-owned save/reboot handling. Other profiles retain their existing manual filesystem path; this work does not establish its atomicity or hardware acceptance.

The updater does not migrate partitions, replace shared LittleFS, restore backups or intentionally reset radio identities, calibration or rolling counters. Ordinary operation may advance counters during a background download. Two application slots do not make shared-filesystem replacement atomic. S3/C5 and 16 MiB P4 update profiles use `default_8MB`; the 4 MiB early-P4 update profile uses `esp32_psram_4MB`. Legacy P4/P4ES targets retain `esp32_3MB` and do not include GitHub updates. Layout mismatches fail closed.

The build hashes its actual generated rollback-enabled bootloader. Runtime hashes the installed bootloader bytes and refuses updates or health confirmation if they differ; the manifest must require that same digest. Read/hash failures and unsupported encrypted/custom bootloaders fail closed. The updater never writes the bootloader.

A new image remains pending until continuous postboot health checks pass, including the matching assets/bootloader, filesystem/configuration, radio availability, network, loop servicing and internal-memory reserve. Health timeout triggers rollback. These checks do not prove physical RF/motor behavior, complete power-failure recovery or counter continuity; existing issue #9 recovery evidence cannot replace live acceptance of this feature.

## Approve a specific service window

After independent review and successful host/profile builds, prepare a dated request identifying exact artifacts and recovery operations. Sid's approval must precede controller access, provisioning, reboot or live tests. [The ADR evidence matrix](github-update-validation.md) separates offline evidence from the remaining live measurements.

1. Record reviewed source revision, stable release tag, firmware/manifest/filesystem hashes, current identity, partition evidence and bootloader attestation. Publish the reviewed release only with publication authorization. A current filesystem lacking the new asset manifest, or a move from a legacy P4 layout, requires **coherent initial provisioning of matching firmware, web assets and layout**, with the exact sequence and recovery path approved in advance. The new 4 MiB early-P4 layout reduces filesystem capacity to 384 KiB: prove that retained current state and packaged assets fit before any write, and preserve unknown/current files privately rather than silently dropping them. Do not use a firmware-first/two-reboot workaround around health checks.
2. Pause motion automations and ensure shutters are stationary during the approved period. Capture fresh full flash and portable configuration/state privately, including linked-remote/shade counters. Record subsequent commands; stale snapshots must not restore stale counters.
3. Prefer an identified spare exact board for fault injection, without simultaneously running duplicate radio identities. Test certificate/hostname/time failures, redirects, truncated/oversized/corrupt/wrong-board/wrong-layout/wrong-bootloader/incompatible-assets downloads, slow/unreachable servers, connection loss, repeated attempts, cancel/retry and memory fragmentation.
4. Record sampled internal minimum/largest block, TLS/PSRAM allocation peaks and worker stack headroom. Measure radio scheduling and web responsiveness during background work and the bounded installation pause. Confirm no unexpected motor command.
5. Interrupt staging/inactive-slot writing, fail first-boot health and verify rollback. Compare filesystem/NVS/identities/calibration/counters before and after success and rollback. Exercise the approved known-good recovery path without restoring stale counters.
6. Load the approved clean stable-tag candidate on the in-service controller only after the agreed prerequisites pass. Observe state continuity and responsiveness, then perform only the separately authorized RF/motor checks. Keep HA installation controls disabled until the complete supported path has passed.

Record actual measurements and unresolved failures. No service-window step has been performed by this development task.

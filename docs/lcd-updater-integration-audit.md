# LCD GitHub updater integration audit

Read-only dependency audit for issue #25, 2026-11-08, based on local main `3d69c8c`. This is a proposed integration sequence, not approval of the historical #12 implementation or evidence of installation. No controller, serial port, private backup, HA, RF or motor access occurred.

## Product decision carried forward

Sid subsequently directed **one shared full-feature firmware for standard T-Embed CC1101 and CC1101 Plus**, because this project's used hardware is the same and nRF24 is unused. This supersedes issue #25's earlier two-canonical-build requirement. The registry should describe the shared supported CC1101/Plus hardware, memory, partition layout and features, rather than publish duplicate binaries. Retain explicit supported hardware membership; this does not authorize generic S3 substitution or silent transition from historical Plus/suffix profile IDs.

## Issue and branch evidence

The complete [issue #12](https://github.com/SidShaytay/ESPSomfy-RTS/issues/12) and both visible comments were read through Sid's authorized human-account GitHub CLI. It remains open with implementation/publication/live acceptance unchecked. The November 7 comment explicitly says the earlier quiet-maintenance state no longer exists and later deployment requires fresh capture and authorization; `a4d003b` was not installed. Old backup, disabled-radio and app-slot assumptions must not be reused. Missing machine-account comments were not recreated or treated as nonexistent.

Inspected without editing these worktrees:

| Worktree | Exact committed head | Actual uncommitted state |
| --- | --- | --- |
| `issue-12-updater` | `baf01473f28a67e689b0a2565c78d83903a784b3` | `platformio.ini`, `tools/preview-local-ui.sh`; unfinished two-canonical-full-feature edits |
| `issue-12-release` | `89727096d51e0583d469b1ae0df3f47a2b7d984a` | Clean |
| `issue-12-radio-guard` | `f22a6ebbc758f084fe55d56c3c29d42fdcbff655` | Seven documentation files, including `AGENTS.md`, anticipating canonical builds |

The updater branch differs from main `3d69c8c` across 132 files, with 5,684 additions and 1,265 deletions. A wholesale merge would replace current build guards/labels, remove issue #25 design records, and regress the reviewed shared-group STOP and caller-owned parser-error changes. Extract/reconcile narrow reviewed components instead; branch presence and historical test claims do not establish accepted integration.

Useful committed milestones (all on the inspected #12 ancestry):

- `5a971a3`: clean compiled release identity and offline packaging.
- `c9802fd`: exact-board PSRAM staging worker.
- `3ebe370`: TLS allocation policy and pre-install state preservation.
- `e8f5636`, `c61137c`, `c35ef07`, `d1e8e48`: installed/packaged bootloader attestation and compiled identity binding.
- `cf8409a`: exact packaged web-asset compatibility.
- `1fe76a7`: production worker fake-transport tests.
- `d393c04`, `09dbb05`, `bb36185`, `d945e51`, `eea9cb5`: broader PSRAM profile registry/tooling; these extra boards are not required to implement the shared LILYGO LCD workflow.
- `a023f45`: exact OTA partition semantics; `c085176`: internal-memory admission; `8b70505`: pending-work quiescence; `025cc4f`: complete authenticated manual-upload activation.
- `bfda38e` on the radio-guard branch fixes a historical WebSocket maintenance authentication mismatch. Its maintenance helper is not a normal updater dependency and must not be run against the controller during development.

## Existing service to reuse

No second LCD installer is needed. The historical `GitUpdateClass` in `src/GitUpdate.h/.cpp` exposes thread-safe check/install/cancel, protected status, exclusive manual/GitHub ownership and loop preparation. Add a typed LCD adapter to that reviewed service after it is integrated. It must call the service directly; do not call HTTP handlers through loopback.

| Dependency slice | Existing sources and behavior to review | Required integration tests |
| --- | --- | --- |
| Profile and release identity | `GitUpdateProfile.h`, `GitUpdatePolicy.*`, `GitUpdateIdentity.*`; `update-profiles.json`, `tools/update_profiles.py`, `tools/update_identity.py`, `app_version.py` | Shared hardware allowlist, canonical flags, unknown/retired IDs, actual compiler defines, exact source/tag/layout marker; preserve new main build guards |
| Deterministic asset compatibility | `tools/update_assets.py`, `minify.py`, `GitUpdateAssets.*`; `/update-assets.txt` hashes each packaged asset | Changed/missing/truncated/oversized asset/manifest, safe paths, same-version different bytes, untouched config/counters |
| Bootloader attestation | `tools/update_bootloader.py`, `update_bootloader.py`, generated `GitUpdateBootloaderExpected.h`, `GitUpdateBootloader.*` | Actual SDK rollback flag/address, actual bootloader bytes, generated marker/digest, wrong/read/hash failures, encrypted/custom unsupported cases |
| Trust/compatibility policy | `GitUpdatePolicy.cpp:28-63` validates schema, board/layout/memory, exact asset name, stable version, digests, web compatibility; HTTPS-only host allowlist | Wrong hardware/layout/features/assets/bootloader, malformed versions/digests/URLs and redirects, no generic fallback |
| TLS and bounded worker | `GitUpdateMemory.*`, `GitUpdateTransfer.h`, `GitUpdate.cpp:141-398` | Allocator overflow/failure/no-internal-fallback, transport failure/timeout/cancel, size/digest/image checks, internal-stack/chunk handling, repeated operations |
| Loop-owned install preparation | `SomfyController.cpp:59-107,249-340`, `UpdateSnapshot.h`, `LoopQueueGate.h`, `LoopQueue.*`, `RadioUpdateActivity.h`, `Transceiver.*`, `ShadeBehaviour.*`, `RadioScheduler.h` | All queued/delayed/TX/RX/GPIO work drained/refused, producer races, allocation/write/readback/rename failures, resume on pre-install refusal, no request after frozen snapshot |
| Manual upload/restore exclusion | `ManualUpload.h`, `RestoreUpload.h`, `RebootRequest.h`, `RebootDeadline.h`, `entryPoints/HttpRoutes.cpp` | Complete authenticated one-file ownership, competing/cancelled/refused clients, no early activation, restore exclusion, bounded failure restart |
| Boot health and rollback | `GitUpdateHealth.h`, `SomfyController.cpp:314-339` | Continuous healthy loop/network/filesystem/config/assets/bootloader/radio/memory interval, reset on unhealthy pass, timeout rollback; preserve disabled-radio maintenance semantics |
| Web and LCD adapters | Existing #12 exact GET status / POST check-install-cancel routes, `data-src/js/github-update.js`; new LCD Settings → Firmware adapter | Shared state/result semantics, fresh local authorization and confirmation, expected candidate binding, queued/busy/cancel/error progress, sensitive-screen capture policy |

The worker currently has one 12,288-byte internal stack on core 0, 180-second overall operation limit, 3-second socket timeout, at most four URL attempts, 65,536-byte release metadata and 4,096-byte manifest buffers. Large TLS allocations (at least 1,024 bytes), metadata and full firmware staging explicitly request PSRAM; the flash writer uses a 4,096-byte internal chunk. Admission requires 48 KiB internal free and a 16 KiB largest block after worker creation. Postboot health separately retains 24 KiB. These are source policies, not proven adequate live headroom alongside LCD/capture/TLS/staging.

## Trust and consequential decisions for Sid

The historical code makes concrete choices, but issue #12 does not record their final acceptance:

1. **Publisher trust.** `GitUpdatePolicy.h` fixes `SidShaytay/ESPSomfy-RTS`; `GitUpdate.cpp` obtains GitHub API asset SHA256 digests over verified HTTPS and binds both manifest and image to those digests. There is **no independent signing key, signature verification, pinned publisher key or key-rotation mechanism**. Recommended minimal policy is to state this GitHub-account/repository trust explicitly and require immutable reviewed releases; if Sid wants compromise protection independent of GitHub/account control, design signed manifests and key rotation before implementing installation. Do not label API digests publisher signatures.
2. **Release channel/version.** Existing policy uses published stable `vMAJOR.MINOR.PATCH` releases only, rejects prereleases/drafts/leading-zero/overflow versions, and installs only newer stable versions. Dirty/untagged builds are `dev-*` and cannot install stable updates. Agree first reviewed fork version/channel and whether LCD only checks latest stable (recommended initial behavior); do not silently invent a beta/downgrade selector.
3. **Time bootstrap and CA updates.** Existing worker uses the pinned framework CA bundle and hostname checks on every allowlisted HTTPS hop, refusing wall time before compiled source time. NTP remains the existing unauthenticated network time dependency. No insecure fallback exists. Accept that bootstrap/recovery policy explicitly or design a stronger time/bootstrap path; explain clock/certificate failures on LCD without disclosing redirect query strings.
4. **Shared hardware identity transition.** Use one shared supported CC1101/Plus profile/image. Choose its stable profile/asset identity before publishing. Old Plus, standard-only and suffix IDs must get a deliberate compatibility transition, not a silent alias to a generic image. Firmware/metadata need an explicit shared `compatible_boards` policy with identical reviewed memory/layout/features and both physical variants, preserving unknown-board rejection.
5. **First updater bootstrap.** Existing code refuses check/install when installed bootloader attestation or exact web-asset manifest is missing, and boot health also requires both. A firmware-only bootstrap onto unmatched existing LittleFS can therefore roll back. No live state was inspected. Prepare coherent initial firmware/assets/bootloader provisioning only after exact state inspection and separate authorization; do not use a firmware-first/two-reboot workaround or assume old bootloader hashes match.

The local unlock scope and browser-owned file workflow are already recorded in [the interaction/security design](lcd-interaction-security-design.md). They do not resolve publisher trust or authorize publication/live writes.

## Required corrections before accepting integration

- **Bind confirmation to the candidate.** Current `requestInstall()` accepts no version/digest argument (`GitUpdate.cpp:432-440`), then `install()` refreshes `/releases/latest` (`:318-321`). A release can change after the LCD displays a version and before installation. Add a typed expected candidate/version/digest/generation to the shared service; if refresh differs, return Changed and require a fresh confirmation. Do not install a different newly discovered version under the earlier confirmation. Apply the same service behavior to web and LCD.
- **Reconcile the registry with the new shared-build decision.** Historical canonical Plus and standard entries forbid display/UI/commands, while three suffix entries permit reduced profiles. The dirty two-canonical edit does not update registry/packager policy and is itself superseded by Sid's one-build decision. Remove retired registry IDs from release selection, require full flags/dependencies and exact shared hardware membership, and rerun final compiler/package negative tests. Do not copy its `lilygo_manifest.py` over the newer main validator.
- **Keep current main safety changes.** Reconcile request/parser API changes, new local authorization and checked persistence work as they land. Broad old copies of `Proto*`, `LocalUiAdapter`, `SomfyController`, `platformio.ini` and docs would overwrite accepted fixes. Review new dependencies individually rather than accepting branch ancestry as proof.
- **Review task ownership in manual paths.** Historical `ManualUpdateEffects::quiesce()` directly calls transceiver/MQTT end methods from the HTTP upload path (`HttpRoutes.cpp:89-96`) while state commit/reboot are queued. Independently establish those task/lock guarantees or move preparation through the shared loop-owned service; do not claim all preparation is loop-owned merely because the GitHub worker callback is.
- **Use a typed bounded status snapshot for LCD.** Existing `status(JsonObject)` is an HTTP-facing serialization method and performs environment admission checks. Add a non-JSON snapshot/result interface owned by the same service; it should carry source/version/compatibility/reason/progress/cancellability and expected candidate identity. Avoid temporary JSON documents or network calls in rendering/input paths.
- **Do not overstate rollback.** SDK config and installed-bootloader hashing prove matching bytes/configuration, not physical power-loss recovery, counter continuity or absence of unexpected RF. Current filesystem replacement remains browser-owned; verified firmware-only update cannot make mounted shared LittleFS atomic.

## Feasible reviewed integration sequence

1. Finish the shared canonical build/product identity and local authorization/persistence prerequisites on latest main. Present the five trust/bootstrap decisions above to Sid before dependent installation flows.
2. Integrate pure profile/manifest/version/URL policy and deterministic asset/source/tag metadata, including the shared supported hardware list and negative tests. Keep network/installation inactive until dependent layers exist.
3. Integrate bootloader generation/attestation and PSRAM allocator hooks against the pinned SDK, then narrow worker policy/transfer tests. Preserve generic no-updater builds and avoid adding unrelated C5/P4 profiles solely to deliver the LILYGO LCD feature.
4. Integrate exclusive admission, loop snapshot/quiescence, manual/restore serialization and boot-health behavior as separately reviewed slices. Inject all failure paths with fake storage/radio/OTA/clock boundaries; preserve STOP/request backpressure and current settings results.
5. Integrate the existing single worker with expected-candidate confirmation and typed status. Wire both web and standalone LCD adapters to it. Display source, installed/available version, compatibility reason and noncancellable phase honestly. Back/cancel must not promise undo after activation or imply a transmitted RF action can be reversed.
6. Build/package the shared LILYGO image and representative generic target; run full native/web/tooling/renderer/fake-service checks and independent review, then integrate finished slices into main. Prepare exact clean tagged release assets locally for review; publication still needs explicit authorization.
7. Keep installation/health/memory/latency/hardware acceptance pending. After authorization, inspect actual hardware/layout/slot/bootloader/assets, capture fresh verified recovery/counters, and execute only the approved service-window plan. No HA install control or RF/motor test is implied.

## Verification performed for this audit

Tests ran against the unchanged historical `issue-12-updater` tree with `PYTHONDONTWRITEBYTECODE=1`, temporary host outputs and the existing compiler wrapper. They supply evidence to review, not acceptance of that branch or a test of the future shared-profile integration:

- `python3 -m unittest discover -s tools -p 'test_update_*.py'`: **148 passed** (profiles, assets, bootloader, packaging, release/source/tag policy).
- `test-update-memory.sh`, `test-update-quiesce.sh`, `test-update-bootloader.sh`, `test-update-assets.sh`, `test-manual-upload.sh`, `test-restore-upload.sh`: **passed** with fake boundaries.
- `test-update-worker.sh`: the production worker passed **298 assertions for each of nine historical registry profiles**, including the now-retired suffix profiles. These runs must be replaced by the new shared-profile matrix; they do not justify keeping retired products.
- Historical worktree tracked status was rechecked and preserved. No firmware build, controller/network-device connection, release publication or physical qualification was performed in this audit. Fake transport tests do not establish real CA/hostname/clock rejection, TLS allocation peaks or radio/web responsiveness.

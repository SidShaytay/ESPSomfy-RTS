# Inactive GitHub update service and installer draft

`GitUpdateClass` is the single shared service reconciled from the issue #12 audit. It supplies latest-release Check, candidate-bound metadata Prepare, and installation through the controller adapter described below. Canonical startup initializes it and its loop adapter without scheduling network requests. Web/LCD controls remain unfinished. `prepared` means verified metadata only; `canInstall` remains false unless all installation callbacks are supplied. Web and LCD adapters must use this service rather than create another installer.

## Ownership and candidate confirmation

`begin(admission, hooks)` and `loop()` belong to the main loop. The typed admission callback runs only there, immediately before worker notification. Requests, snapshots, cancellation and external ownership tokens are thread-safe. A request acquires the one shared owner and queues admission without allocating or doing I/O. The worker owns transport, metadata and flash writing, never model/settings/radio/GPIO state. Manual uploads and restore use this same owner and transfer lifecycle. Their acquisition invalidates any cached candidate. A stale or mismatched token cannot release another owner.

Cancellation retains ownership until transport cleanup, document/buffer destruction and measurement finish. Worker notifications and SDK calls occur outside the status critical section. Snapshot copies are bounded and contain cached metrics; status serialization performs no network or filesystem I/O.

A candidate binds source SHA, stable version, image size and SHA256, manifest SHA256, and generation. Generation starts from a bounded, nonzero 63-bit random boot nonce and increments without wrapping. It supplies opaque freshness, not authentication; no NVS writes are needed. An all-zero seed makes the service unavailable. JSON encodes generations as decimal strings so browsers preserve all bits. Refresh and external ownership invalidate old confirmation. Prepare first requires the exact cached candidate, then refetches authenticated latest metadata and manifest: an identity change produces `changed` and a new generation. Failure/cancellation clears the candidate. A later installer must independently retain this binding through final admission.

Each accepted check, preparation and installation also receives a distinct
boot-local operation generation from the same monotonic sequence. It differs
from the confirmed candidate and private storage-owner token.
`operationGeneration` is serialized as a decimal string. LCD cancellation binds
the exact operation and its original session/security grant; a closed or stale
screen cannot cancel a newer operation. Expiry stops owned reversible work
without extending the session. Exhausted freshness refuses before network or
flash work and never wraps or substitutes a fixed identifier.

## Fixed trust and bounded transport

Only the shared CC1101 / Plus profile is supported. An untagged/development installed identity is unavailable. Before network I/O, the worker checks wall time against the compiled source commit epoch, actual flash/PSRAM, exact partition inventory and running/next OTA state, installed bootloader digest, and installed immutable bundled web assets. Missing prerequisites produce typed unavailable/error reasons; they do not authorize bootstrap or inspection of a controller.

The repository is fixed to `SidShaytay/ESPSomfy-RTS`. Verified HTTPS requires the SDK CA bundle, hostname verification, and the [updater certificate-date adapter](github-update-certificate.md). Every redirect is checked against the existing strict HTTPS host policy, with at most four URL attempts. Release API and HTML identities must still name the fixed repository. Metadata must describe a published, non-draft, non-prerelease stable release with exactly one canonical image and manifest asset. API SHA256 digests authenticate the manifest bytes before parsing. The schema-3 bundled-assets manifest cross-binds source, tag, firmware identity, shared profile/features, layout, installed bootloader and assets. Unknown fields do not weaken required fields. No raw server content, location, query strings or credentials enter status/error logs.

The operation deadline is 180 seconds, with a three-second socket timeout and cooperative cancellation during network reads and local attestation. API bodies are limited to 64 KiB; manifest bodies to 4 KiB. JSON has a 192 KiB requested PSRAM budget. Chunked API metadata is bounded; manifest length must be positive and match its authenticated API size. Duplicate/oversized Location headers, non-identity content encoding, truncation, malformed types, embedded NULs, oversized JSON allocation and unauthenticated digests fail closed. Bodies/JSON use PSRAM without internal fallback.

The worker requests a 12 KiB internal stack and checks at least 48 KiB free internal memory with a 16 KiB largest block before transport. The global mbedTLS allocator caps requested bytes, including its own headers, at 512 KiB total and 32 KiB internal. These are requested-byte budgets, not SDK physical overhead claims. Blocks of at least 1024 bytes require PSRAM; smaller blocks require internal memory. Allocation failure has no fallback; free wipes the full allocated block.

## Activation prerequisites

The mbedTLS allocator hook is global. Canonical startup installs it **before any mbedTLS allocation**, including the web API token HMAC, display/capture or web initialization. Switching after existing allocations would mismatch allocator/free pairs. Failed startup initialization leaves the updater unavailable and does not trigger a later hook switch. Token HMAC failure refuses authentication. The certificate adapter itself is per-client: synchronous HTTP cleanup completes while its callback context is alive, and other clients keep their own bundle callbacks.

The controller callbacks supply fresh authorization, model admission, quiescence, cleanup and the final SafeRestart handoff. User-facing confirmation and managed rollback remain unfinished. [Continuous boot health](github-update-boot-health.md) now qualifies confirmation and emits a deadline failure intent. Existing settings durability tracks uncertainty by complete persisted namespace (general/connection share CFG); installer admission uses it. Historical bootloader/assets migration is a separate explicitly authorized stage. There is no live access or provisioning authorization here.

## Offline evidence and limits

The isolated installer draft reuses this worker and its exclusive owner. With
loop-owned callbacks supplied, it re-fetches the exact confirmed release,
stages its image in PSRAM, validates the complete bundled firmware, and writes
only the inactive OTA slot using a 4 KiB internal buffer. It never selects a
boot partition. Cancellation signals an active worker; controller cleanup waits
for SDK I/O and the transfer lifecycle to close. A failed abort retains the
owner and paused resources, with no automatic retry or restart.

`python3 tests/git-update-install/run.py` exercises the production worker with
25 entirely offline worker scenarios: a valid synthetic bundled image, invalid/short image,
cancellation at transfer/begin/write/end, stale authorization, transfer expiry,
allocation and notification failures, SDK failures, and busy cleanup. The valid
case compares every written byte and verifies zero early boot selections.
Activation requires the final restart owner, re-resolves the exact written
address, size and OTA subtype, then checks the SDK boot-partition readback.
No activation is repeated, including after the ordinary transfer deadline.

The same command runs 16 coupled controller journeys through the actual worker,
`GitUpdateController` and SafeRestart. The loop adapter binds the local session,
security configuration, model revision and settings uncertainty history. It
refuses moving/pending work, seals producers, obtains the exact radio pause and
saves the model through the existing checked writer before OTA begins. It keeps
those resources through writing and final selection. Cancellation/expiry resumes
only after closed I/O; busy cleanup retains resources and stale cleanup stops
without retrying a physical operation. The controller requests no RF/motor
commands and stores no credentials in its source record.

Startup invokes the adapter after global TLS allocator initialization. The main
loop services network/UI while `gitUpdateControllerPaused()` and defers normal
model/queue execution until cleanup or final shutdown. LCD controls and managed
rollback remain unfinished; these tests do not establish complete user-facing
updater acceptance.

`tools/test-update-worker.sh` compiles production service/policy/certificate code against scripted SDK boundaries and local SHA256. It exercises valid and changed candidates, stale/reboot generations, cancellation/deadlines, ownership races, malformed metadata/digests/redirects, allocation/resource failures and certificate errors. Fakes assert no critical-section SDK calls, leaks, partition writes or model effects. A mutation removing manifest-digest binding fails the changed-manifest regression.

`tools/test-update-sdk-link.sh` forces the otherwise inactive worker and allocator initializer into a real pinned-SDK ELF without calling them. Its temporary linker-only configuration is not a release package. Normal shared and generic builds remain separate checks. Host date tests plus pinned binary callback/ABI evidence are documented in the certificate guide; no actual TLS handshake, live GitHub request, physical heap/latency, bootloader recovery or hardware operation is claimed. All tests run offline with local fixtures.

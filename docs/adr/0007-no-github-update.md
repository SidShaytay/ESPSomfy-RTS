---
status: accepted
---

# The firmware no longer updates itself from GitHub

The device fetched GitHub's release list, downloaded firmware and filesystem images, and checked daily for a newer release, all over HTTPS on the device itself. A TLS session needs one contiguous block of about 35–40 KB. On IDF 5.5 the heap's largest free block is often smaller (issue #76: "SSL - Memory allocation failed" with a 34.8 KB largest block). The download checked no certificate (`setInsecure()`), so anyone who could intercept it could serve their own firmware. Parts of it ran blocking on the web server's task, and its socket events from there were dropped. The web UI's update buttons sent PUT to routes that accept only GET, so they had been answering 404.

We removed it: the release check and its setting, the release list, download and cancel, `/recoverFilesystem` and the `updateProgress` event. Updates are done by uploading `firmware.bin` and `littlefs.bin` (`/updateFirmware`, `/updateApplication`), or over USB.

## Consequences

- Users download releases from GitHub themselves. The web UI's firmware page shows the installed version and the upload form.
- Home Assistant's update entity can no longer install. From integration 3.0.4 Home Assistant checks GitHub for the latest release itself, and the entity shows when a newer one is out (cjkas/ESPSomfy-RTS-HA#22).
- A broken filesystem is recovered by uploading `littlefs.bin`, or over USB, rather than with `/recoverFilesystem`.
- Issue #76 and PR #72 (GitOTA's per-chip littlefs file name) become obsolete.
- The HTTPS client leaves the firmware, which frees flash in the 1.5 MB OTA slot.

## Fork follow-up: verified PSRAM-board updates (issue #12)

The upstream decision above remains accepted for its original generic builds.
This fork's [verified update design](../github-updates.md) revisits the constraints
for explicit release profiles with at least 8 MiB working PSRAM: verified outbound
HTTPS, deliberate PSRAM allocation/staging, bounded background operations,
consistent authenticated routes, exact-board release identity and web-asset
fingerprints, and firmware-only inactive-slot writes with attested bootloader and
postboot health/rollback checks. The local web UI still uses HTTP.

[Board memory and profile requirements](../board-memory.md) distinguish no-PSRAM
builds, single-slot debug builds, and opt-in PSRAM release targets. Existing
partition layouts are preserved; actual image size must fit each profile's slot
and required headroom. Memory capacity alone never enables installation or
selects another board's binary.

The [evidence matrix](../github-update-validation.md) maps every original
objection to an implemented control and offline verification, and identifies the
remaining live tests. Independent review, measured memory/responsiveness and
fault/rollback/state-continuity tests remain requirements for hardware acceptance.
No controller access, release publication or service-window approval is implied
by this addendum.

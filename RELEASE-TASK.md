# Public LILYGO release — build and installation complete

Objective: prepare a non-engineering build from latest completed main, update user documentation and preserve shutter/radio state during installation.

Baseline: origin/main ef1bd12, fetched November 10, 2026. Release branch/worktree: codex/release-4.1, release-4.1. Completed source integrated into main. Signed release tag v4.1.0 identifies source 3bf61a4c4b1060b7af37781abe6d3eb282cb60f9.

## Completed

- README covers LCD controls/status/navigation page 3, dark mode, security fixes, cjkas baseline improvements, bundled firmware/web assets, PSRAM-backed verified GitHub updates and first installation. Seven production-renderer PNGs use synthetic public data. New docs/first-install.md distinguishes matching-layout firmware upload from state-preserving USB migration; arbitrary firmware formats are not claimed compatible.
- Version 4.1.0 follows Sid's clarification that the only engineering controller is under his control. Its transition from 4.9.1 used a manual upload. Navigation docs distinguish public and engineering version numbers.
- Checks passed: 1,318 native tests (one optional private-fixture skip), 122 JS tests, 96 Python tooling tests, release scripts, board/rejection checks, renderer/runtime/capture/adapter/queue/updater harnesses, clean tagged SDK build, package validation, local links and image sizes. Review: build/release-review.md.
- GitHub release v4.1.0 is published and explicitly marked latest: https://github.com/SidShaytay/ESPSomfy-RTS/releases/tag/v4.1.0 . Its exact firmware and update manifest were re-downloaded and matched the installed candidate, with API SHA256 digests.
- Sid explicitly authorized flashing the attached controller. Fresh full 16 MiB backup verified layout MD5, both app checksums/SHA256, NVS CRCs, configuration and effective counters. One /updateFirmware upload wrote inactive app0. Exact first boot and SDK health confirmation preceded further serial control. Full post-readback verified VALID OTA25/app0, exact release image, entire old app1, bootloader/table/coredump/upper flash, all 16 NVS namespaces/counters and every LittleFS file. Normal reboot and all five live settings/entities/rooms/counters matched. Passive logger restored. No RF, motor or HA commands were sent.
- Private recovery evidence: ~/.local/state/espsomfy-lilygo/CURRENT_PUBLIC410_INSTALL points to public410-install-xxr_1eo4; installation-complete.json and SHA256SUMS bind the evidence. Do not restore old counters blindly.

## History review and remaining decisions

Sid requested long squashable sequences while retaining a unique commit per major feature. Review of d643217..ef1bd12 found 627 commits (141 merges). Merge 906ee81 reintroduced the original LCD development branch after consolidation ceb58b5. Exact feature boundaries and cleanup order: build/history-review.md, also preserved in the private archive.

Verified archive: ~/.local/state/espsomfy-lilygo/engineering-release-archive-20261110 contains old main, all seven engineering tags in a verified Git bundle, three release listings/assets and checksums. Sid approved publication and engineering cleanup. Deleted the v4.8.0, v4.9.0 and v4.9.1 release listings, and the local/remote v4.7.0, v4.7.1, v4.7.2, v4.7.3, v4.8.0, v4.9.0 and v4.9.1 tags after verifying the archive checksums/bundle. Remote tag deletion used an atomic push with explicit expected-object leases. GitHub now lists only published v4.1.0 and its latest API returns v4.1.0. Main history was not rewritten. Preserve signed v4.1.0 and original deployed-source identities if history is later reconstructed; do not retag installed binaries.

Release publication and engineering cleanup are complete. History reconstruction remains a separate proposed task: prepare any cleaned history on a separate branch, preserve each major feature/security fix, and check final tree equality before requesting published-main replacement. Physical controls/other-board/combined-load qualification remains tracked in issue 33; this release does not claim those outcomes.

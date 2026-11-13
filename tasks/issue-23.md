# Issue 23: native group percentage integration

Objective: provide native RF group percentage movement on current main, with matching Home Assistant capability detection and legacy fallback.

Acceptance: preserve existing protocol IDs; validate groupTarget requests; use one shared group start when eligible, individual timed stops, safe queued-command replacement and reversal; production-path regression coverage; current native/web checks and supported firmware builds pass; reviewed changes integrated into main. Physical acceptance remains separately gated.

Scope: firmware and companion HA source/tests. Sid subsequently authorized attached-board access, backup, flashing/reboot and any RF commands for two hours (through 2026-11-12 20:40 PST). HA deployment/restart still needs separate authorization.

Workspace: codex/issue-23-integration, based on origin/main a944911. Prior reviewed implementation: 5cbe11b; historical HA implementation: 34eb042. The maintained firmware baseline and scheduler have changed; port only feature changes.

Milestones:
- [x] Reproduced missing native targets on current main; reconciled type 57 (55/56 preserved), production receipts/counter/GPIO guards, queued/on-air cancellation, timed stops and retargeting.
- [x] Validate and commit firmware integration: scheduler, timing and protocol slices committed; production integration passes 29 scenarios.
- [x] HA b396171 reviewed, 142 tests passed, integrated and pushed to main. Fresh live inspection: all 21 installed file hashes already match; no replacement/restart necessary. Sid authorized HA deployment/restart if needed.
- [-] Complete final integration checks, merge firmware main, build clean package and install/test within authorized window.

Completed milestone: production harness found and reproduced stale queued Stops, undelivered start-event Stop loss, and refused overrides erasing accepted reversal plans. Repairs pass those core cases; review continues. HA source and 19 independent regressions pass 142 HA/protocol tests. Canonical firmware builds. Fresh portable capture and complete 16 MiB dump validated: exact layout, VALID active app1 source56e51ed, both image hashes/checksums, NVS CRCs, configuration and effective counters. Prior confirmed image restored; passive USB logger running. Ordinary group tilt/short-step and favourite-programming regressions were reproduced and repaired before deployment.

Software verification: independent movement and HA review completed, including mutation tests. 29 production movement cases and 13 inherited group/restart cases pass; GPIO, owned-repeat, explicit counter, model allocation/load and board-profile checks pass; 123 JS and release-script checks pass. Canonical clean package and esp32dev build pass. Concurrent PlatformIO processes collided in their shared build directory; generic build and the two interrupted native suites were rerun in isolated directories.

Next: integrate tested firmware source into main, regenerate exact clean package after final test-harness commit, refresh admission capture and install to verified inactive app0. Then compare direct protocol, HA and Apple Home behavior; record observed motors separately from estimates. Private recovery pointer: ~/.local/state/espsomfy-lilygo/ISSUE23_RELIABLE_ACTIVE.

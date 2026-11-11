# Issue 35 — complete

Objective: complete the firmware stack audit and publish sanitized, reproducible measurements. GitHub #35 is authoritative. Broader physical/full-load acceptance remains #33.

Authority: Sid authorized completion, necessary full backups, instrumented installation/reboots and bounded read-only measurement; requested appropriate subagents and efficiency. Root alone operated board/USB. No RF/motor/relay commands, HA changes, pairing/calibration, deliberate Wi-Fi disruption, rollback faults or duplicate controller identity.

Workspace: stack-audit / codex/stack-audit. Code fixes are integrated/pushed to main; exact installed firmware is clean v4.1.2/source a945a59 (public source tag, no binary release). Private evidence/recovery pointer: ~/.local/state/espsomfy-lilygo/ISSUE35_ACTIVE; first phase: ISSUE35_PHASE1. History: stack-audit.log.

- [x] S1 Baseline inventory/compiler/binary audit:273 .su files,2074 functions; final instrumented2081. Public datasets bind source/config/ELF and distinguish frames from task usage.
- [x] S2 Opt-in self-sampling for6 roles and sanitized exporter. Release-level output failure reproduced/fixed; emitted-binary gate prevents silent probes. Actual probe frame48B; logging latency unmeasured.
- [x] S3 Reviewed SSDP handoff/deadline/lifecycle fixes, versioned source checkpoints, clean normal/instrumented/shared+generic builds. Native1318passed/1existing optional skip; focused production-method/concurrency, collector/privacy/provenance, release-level/default-disabled, fresh-build and version tests passed. Reviews approved.
- [x] S4 Two fresh full16MiB backups at4M verified layout, both app checksums/SHA, OTA state, NVS CRCs, LittleFS configuration and effective counters. Guarded bootbacks/state comparisons passed. Exact4.1.2 app1 boot/SDKconfirmation and final settings/entities/rooms/counters unchanged; previous app0 retained. First4.1.1 image was healthy but logging-silent, so contributes no runtime measurements.
- [x] S5 Reviewed public report/data are committed and pushed in c306189. GitHub #35 is closed with every acceptance item checked. Remote public dataset blob matches the reviewed local file; final live version4.1.2/same-boot/failure checks and active diagnostics service passed. Runtime98records/all6roles;5minidle,5minload,1minrecovery;74syncs,60HTTP200screenshots,6discovery replies. Host top-downBMP assertion corrected after preserving idle/failure evidence; pause disclosed. No unexpected reset/allocation/health markers. Minimum unused bytes:loop2932,network1960,UDP728,web12652,radio3412,updater10856. UDP is narrowest; no stack downsizing. Radio/updater idle only; activeRF/TLS, physical edits, deliberateWiFi reconnect, other boards and long idle remain unqualified.
  No required work remains in #35. Keep #33 separate for unexercised physical/full-load acceptance. Future writes require fresh identity/state/recovery checks; never restore dated counters.

Next ID: S6.

# Remaining stack qualification — active

Objective: execute and publish reproducible real-board stack coverage for active RF, updater TLS/download/staging, local UI editing, deliberate Wi-Fi reconnect, combined activity and two-hour recovery, including feasible SDK task observations. Follow-up to completed #35; authoritative qualification issue #33.

Authority: Sid requests planning and execution, prefers automated USB debug controls, authorizes necessary harmless RF (open while already open), and confirms shutters physically open now. Existing session authorization covers necessary full backups, instrumented installs/reboots. No pairing/calibration, limit changes, relay tests, HA configuration or stale-counter restore. Revalidate state before RF; counters may advance legitimately and must be preserved.

Workspace: stack-qualification / codex/stack-qualification, starting main c2e2f86. Root owns live board and record. Subagents perform read-only USB/API and SDK measurement research; no live operations.

Acceptance: exact boot/source binding; bounded repeatable controls using production admission and UI presentation; per-phase stack/heap/TLS/PSRAM/timing and cleanup; no unexpected resets/unintended persistent edits/movement; public sanitized outcomes with untested physical/alternate-board boundaries explicit. No stack resizing in this effort.

- [x] Q1 Inventory and fresh live idle/v4.1.2 capture passed. Executable phase plan posted on #33; completed #35 baseline split into a checked item, extended combined-load remains open.
- [x] Q2 Add/test/review opt-in USB control and SDK sampling support; preserve ordinary builds and protection gates. Source and focused tests/reviews passed; controlled development build passed (2094 repository frames), native1318pass/1existing skip after correcting host compiler PATH. Host ACK binding defect reproduced/fixed. Integrated/pushed main2136bf2/tagv4.1.3; clean default/controlled builds passed. Additional host read-boundary ACK race is being fixed before USB use.
- [-] Q3 Fresh16MiB read verified bothapps/layout/NVS/config/counters, actualVALID28app1/current4.1.2 and retained27app0. Exact bootback/state passed. Clean candidate2136bf2/v4.1.3 hashes/bootloader/table/targetapp0seq29 independently approved; actualread-only observerFD guard added. One-shot firmware upload now executing; never retry/reset ambiguously. Next: exactfirstboot/SDKhealth/state.
- [ ] Q4 Execute isolated then combined phases and two-hour recovery; preserve failures and stop escalation on concrete regressions.
- [ ] Q5 Publish reviewed sanitized report/data; reconcile #33 checkboxes and remaining physical coverage.

Private current stage: ~/.local/state/espsomfy-lilygo/CURRENT_STACK_QUALIFICATION. Baseline: ISSUE35_ACTIVE. Never run old one-shot install helpers unchanged. Next ID: Q6.

# Issue 25 execution record

Objective: implement full LCD control/administration and one shared full-feature LILYGO build for CC1101 and Plus, with independently reviewed increments integrated into main. Issue 25 remains incomplete; Sid's later product decisions below supersede its original build requirement.

Workspace: `/var/mnt/data/projects/personal/ESPSomfy-RTS/issue-25-design`, branch `codex/25-lcd-design`. Started at latest SSH-fetched remote main `fc5a2947928fdf855185f77ecaae69c00b677156`, then fast-forwarded the existing reviewed local main STOP fix `a493d50`. Preserve all existing worktrees, including dirty #12 updater and radio-guard work. No live controller access, flashing/reboot, RF/motor operations, pairing/calibration/limits, HA changes, upstream posting or releases authorized.

GitHub connector is suspended; authorized human CLI account `SidShaytay` verified active. SSH fetch works with `GIT_CONFIG_COUNT=0` and `GIT_SSH_COMMAND='ssh -i /var/home/sid/.ssh/id_ed25519_2026_vega -o IdentitiesOnly=yes -o BatchMode=yes'`. Do not print key material. Issue has no comments as read 2026-11-08.

1. [x] Design inventory (root integration owner; agents own separate worktrees).
   - `web_inventory`: `issue-25-web-inventory`, capability matrix.
   - `interaction_security`: `issue-25-interaction-design`, interaction/security design.
   - `build_audit`: `issue-25-build-audit`, canonical build/package audit.
   - Matrix, interaction/security and build audit integrated; cross-review by inventory/security agents found persistence-outcome and backup wording gaps, corrected. Production baseline renderer previews generated; proposed new menus remain text mockups until implemented.
2. [x] Agree authorization/transfer UX; latest steering supersedes earlier answers.
   - Approved: reuse configured web credential for local controls AND admin, proposed 2-minute inactivity timeout and device-wide attempt limit; security None remains open with risky-action confirmations.
   - Approved latest: keep STOP after expiry only for the already-authorized current movement until it ends; context/settings change revokes, and no later motion gains authority. Other actions lock. Existing web throttle schedule reused independently; volatile reboot reset is documented.
   - Approved: file-based backup/restore/manual firmware/web uploads remain browser-owned equivalent workflows. No parallel hybrid approval/staging/encryption feature. LCD owns fully standalone operations, including #12 verified GitHub service after reviewed integration.
   - Latest build decision: one shared firmware build supports CC1101 and Plus. Their used peripherals/wiring/memory are equivalent; Plus nRF24 is unused. This supersedes the issue's original two-build requirement. Consolidate profile/package/CI/docs while retaining explicit compatibility and deliberate old-profile transition checks.
   - Next: implement approved flows with fake request sinks; never infer hardware authorization.
3. [-] Implement reviewed increments. No feature deferrals approved.
   - Active owners: interaction_security in issue-25-interaction-design (unlock/editor/capture + generic movement STOP lease); web_inventory in issue-25-settings-results (checked NVS persistence/shared typed outcomes). Root integrates/reviews and owns subsequent dynamic catalogue/domain/admin work; build_audit available for updater dependency audit.
   - Completed reviewed increments: caller-owned validator errors (2b0114c), canonical boards/build/package guards (9f38354), current docs and size evidence. Root review found feature-value/path gaps, corrected before integration. Root focused 63 parser tests, 34 Python tests and both-board/generic compile/panel/radio harnesses passed; agent full native 665 pass/one optional skip, production renderer/runtime/capture and STOP tests passed.
   - Both full canonical firmware/filesystem packages with parser fix validated; bin sizes standard 1,407,344 / Plus 1,407,376, OTA headroom 1,934,992 / 1,934,960 bytes. Generic ESP32 firmware passed. Generated packages record dirty source honestly and are not releases. See build audit for exact evidence.
   - Next: review tested commits, integrate completed increments into main; preserve reviewed moving/mixed/idle group STOP. Unicode label coverage and web light/group-step gaps remain unresolved for affected flows.
4. [ ] Independent review, tests/builds, main integration and staged acceptance evidence.
   - Next: distinguish host/build evidence from pending hardware acceptance. Do not close issue as fully verified.

Baseline at a493d50: 662 native tests passed + one optional private-fixture skip; 69 JS tests; 26 Python tooling tests; release script; board/GPIO, status/radio-SPI, adapter and production renderer/runtime/capture passed. Use existing `rocm-7.2.3` toolbox compiler wrappers in `~/.cache/espsomfy-lilygo/native-toolbin`, pio `~/.platformio/penv/bin/pio` (6.2.0). Set `GFX_DIR` to pinned GFX cache in `issue-12-updater/.pio/libdeps/lilygo-t-embed-cc1101-plus-ui/`. No packages installed. Production previews: ignored `build/ui-preview/`. Full firmware builds and hardware memory/latency acceptance remain pending.

Next ID: 5.

# Issue 25 execution record

Objective: implement full LCD control/administration and one full-feature build per LILYGO physical board, with independently reviewed increments integrated into main. Issue 25 is the acceptance authority; it remains incomplete.

Workspace: `/var/mnt/data/projects/personal/ESPSomfy-RTS/issue-25-design`, branch `codex/25-lcd-design`. Started at latest SSH-fetched remote main `fc5a2947928fdf855185f77ecaae69c00b677156`, then fast-forwarded the existing reviewed local main STOP fix `a493d50`. Preserve all existing worktrees, including dirty #12 updater and radio-guard work. No live controller access, flashing/reboot, RF/motor operations, pairing/calibration/limits, HA changes, upstream posting or releases authorized.

GitHub connector is suspended; authorized human CLI account `SidShaytay` verified active. SSH fetch works with `GIT_CONFIG_COUNT=0` and `GIT_SSH_COMMAND='ssh -i /var/home/sid/.ssh/id_ed25519_2026_vega -o IdentitiesOnly=yes -o BatchMode=yes'`. Do not print key material. Issue has no comments as read 2026-11-08.

1. [x] Design inventory (root integration owner; agents own separate worktrees).
   - `web_inventory`: `issue-25-web-inventory`, capability matrix.
   - `interaction_security`: `issue-25-interaction-design`, interaction/security design.
   - `build_audit`: `issue-25-build-audit`, canonical build/package audit.
   - Matrix, interaction/security and build audit integrated; cross-review by inventory/security agents found persistence-outcome and backup wording gaps, corrected. Production baseline renderer previews generated; proposed new menus remain text mockups until implemented.
2. [!] Agree authorization/transfer UX.
   - Approved: reuse configured web credential for local controls AND admin, proposed 2-minute inactivity timeout and device-wide attempt limit; security None remains open with risky-action confirmations.
   - Approved: STOP requires a fresh unlock after expiry, with no exception. Existing web throttle schedule reused independently; volatile reboot reset is documented.
   - Need: Sid is still reading the explanation of file staging/physical approval and encryption; no transfer decision made. Do not hurry or repeatedly ask.
   - Next: incorporate answers into design; no timeout implies approval.
3. [-] Implement reviewed increments. No feature deferrals approved.
   - Active owners: build_audit in issue-25-build-audit (canonical board/build/package contract); interaction_security in issue-25-interaction-design (approved unlock/editor/capture); web_inventory in issue-25-parser-errors (caller-owned validator errors for cross-task safety). Root integrates/reviews and owns subsequent dynamic catalogue/domain/admin work.
   - Next: review tested commits, integrate completed increments into main; preserve reviewed moving/mixed/idle group STOP. Transfer, Unicode label coverage and web light/group-step gaps remain unresolved for affected flows.
4. [ ] Independent review, tests/builds, main integration and staged acceptance evidence.
   - Next: distinguish host/build evidence from pending hardware acceptance. Do not close issue as fully verified.

Baseline at a493d50: 662 native tests passed + one optional private-fixture skip; 69 JS tests; 26 Python tooling tests; release script; board/GPIO, status/radio-SPI, adapter and production renderer/runtime/capture passed. Use existing `rocm-7.2.3` toolbox compiler wrappers in `~/.cache/espsomfy-lilygo/native-toolbin`, pio `~/.platformio/penv/bin/pio` (6.2.0). Set `GFX_DIR` to pinned GFX cache in `issue-12-updater/.pio/libdeps/lilygo-t-embed-cc1101-plus-ui/`. No packages installed. Production previews: ignored `build/ui-preview/`. Full firmware builds and hardware memory/latency acceptance remain pending.

Next ID: 5.

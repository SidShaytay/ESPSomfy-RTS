# Issue 25 execution record

Objective: implement full LCD control/administration and one shared full-feature LILYGO build for CC1101 and Plus, with independently reviewed increments integrated into main. Issue 25 remains incomplete; Sid's later product decisions below supersede its original build requirement.

Workspace: `/var/mnt/data/projects/personal/ESPSomfy-RTS/issue-25-design`, branch `codex/25-lcd-design`. Started at latest SSH-fetched remote main `fc5a2947928fdf855185f77ecaae69c00b677156`, then fast-forwarded the existing reviewed local main STOP fix `a493d50`. Preserve all existing worktrees, including dirty #12 updater and radio-guard work. No live controller access, flashing/reboot, RF/motor operations, pairing/calibration/limits, HA changes, upstream posting or releases authorized.

Current local main is `801b080` (parser/shared build/checked settings integrated). Root integration contains independently approved auth source through `0242213`, plus uncommitted approved 30-second STOP-cap/high-water/renderer corrections. Do not overwrite those pending edits. Local commits use per-command `-c commit.gpgsign=false`; no global Git configuration changed.

GitHub connector is suspended; authorized human CLI account `SidShaytay` verified active. SSH fetch works with `GIT_CONFIG_COUNT=0` and `GIT_SSH_COMMAND='ssh -i /var/home/sid/.ssh/id_ed25519_2026_vega -o IdentitiesOnly=yes -o BatchMode=yes'`. Do not print key material. Issue has no comments as read 2026-11-08.

1. [x] Design inventory (root integration owner; agents own separate worktrees).
   - `web_inventory`: `issue-25-web-inventory`, capability matrix.
   - `interaction_security`: `issue-25-interaction-design`, interaction/security design.
   - `build_audit`: `issue-25-build-audit`, canonical build/package audit.
   - Matrix, interaction/security and build audit integrated; cross-review by inventory/security agents found persistence-outcome and backup wording gaps, corrected. Production baseline renderer previews generated; proposed new menus remain text mockups until implemented.
2. [x] Agree authorization/transfer UX; latest steering supersedes earlier answers.
   - Approved: reuse configured web credential for local controls AND admin, proposed 2-minute inactivity timeout and device-wide attempt limit; security None remains open with risky-action confirmations.
   - Approved latest: keep STOP only for already-authorized continuing movement until it ends OR 30 seconds after original session expiry, whichever is earlier. Context/settings change revokes; later motion gains no authority. Other actions lock. Wake/wrong unlock/delayed polling cannot extend the cap. Existing web throttle schedule reused independently; volatile reboot reset documented.
   - Approved: file-based backup/restore/manual firmware/web uploads remain browser-owned equivalent workflows. No parallel hybrid approval/staging/encryption feature. LCD owns fully standalone operations, including #12 verified GitHub service after reviewed integration.
   - Latest build decision: one shared firmware build supports CC1101 and Plus. Their used peripherals/wiring/memory are equivalent; Plus nRF24 is unused. This supersedes the issue's original two-build requirement. Consolidate profile/package/CI/docs while retaining explicit compatibility and deliberate old-profile transition checks.
   - Updater trust approved: verified HTTPS/GitHub asset digests from SidShaytay/ESPSomfy-RTS; no separate publisher signature. No publication/live bootstrap authorized.
   - Pending: Sid choice on clearly unsupported GPIO relay separate/Euro tilt versus separate output design. Do not map those modes to lift pins. Unicode font coverage and visual-only web light control still require disposition.
   - Next: implement approved flows with fake request sinks; never infer hardware authorization.
3. [-] Implement reviewed increments. No feature deferrals approved.
   - Root `issue-25-dynamic-controls` has uncommitted LocalCatalogue.h/7 passing native cases and model/restore/per-entity revision changes in Somfy*/ConfigFile; not wired yet. Catalogue reviewed, duplicate-ID mutation killed two cases. Reconcile latest main/auth before runtime wiring; no dynamic-parity claim yet.
   - Active/ready agent streams: build_audit `issue-25-control-policy` ready9f15be9+c285461 (group steps, typed policy; native691+skip/shared build), now independent GP_Remote toggle fix from latest main; interaction_security `issue-25-value-editor` ready4dc7409+3f99343 (bounded drafts/picker, clock fix); web_inventory `issue-25-form-metadata` readyfb695f5+4d181b7 (authoritative descriptors/typed shared read; native691+skip/ETH49/JS69). Root must review and integrate before consumers build on them.
   - Completed reviewed increments: caller-owned validator errors (2b0114c), canonical boards/build/package guards (9f38354), current docs and size evidence. Root review found feature-value/path gaps, corrected before integration. Root focused 63 parser tests, 34 Python tests and both-board/generic compile/panel/radio harnesses passed; agent full native 665 pass/one optional skip, production renderer/runtime/capture and STOP tests passed.
   - Shared image/package consolidation reviewed/integrated; clean author b402621 package dirty:false, bin1,407,360, slot headroom1,934,976. Checked settings801b080 independently reviewed: focused18/shared+generic firmware passed; failed saves don't mutate live settings/reboot/tokens. Historical two-board evidence remains in build audit.
   - Root combined pre-cap check: native697 passed/one optional skip, JS69, Python36, release script, real GFX/runtime/capture/domain+credential adapter passed; auth/settings shared firmware compiled. Latest cap:30 focused native and production host harnesses passed; independent reviewer approved high-water rollback/throttle and large-wait pixel fixes. Next commit cap/docs, final changed-source build/tests, main integration.
   - Next: review tested commits, integrate completed increments into main; preserve reviewed moving/mixed/idle group STOP. Unicode label coverage and web light/group-step gaps remain unresolved for affected flows.
4. [ ] Independent review, tests/builds, main integration and staged acceptance evidence.
   - Next: distinguish host/build evidence from pending hardware acceptance. Do not close issue as fully verified.

Baseline a493d50 native662+optional skip; current evidence above. Tools: existing `rocm-7.2.3` compiler wrappers in `~/.cache/espsomfy-lilygo/native-toolbin`, pio `~/.platformio/penv/bin/pio`6.2.0. No packages installed. Root firmware build dirs `.pio/build-settings-shared`/`build-settings-generic`; pinned GFX now `.pio/libdeps/lilygo-t-embed-cc1101/Adafruit GFX Library`. Generated artifacts/previews ignored; never commit binaries/private state. Hardware memory/latency/RF/physical acceptance remains pending.

Next ID: 8.

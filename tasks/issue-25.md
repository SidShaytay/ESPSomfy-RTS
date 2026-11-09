# Issue #25 — full standalone LCD implementation

Objective: finish agreed LCD/web capability matrix, implementation, tests, docs and independent review; then authorized state-preserving flash. Incomplete. Main defines completion; preserve every existing worktree. Root alone maintains this record. History/evidence: issue-25.log.

## Authority and choices

No live controller/browser/serial/RF/motor/HA access has occurred. Flash is authorized only after complete reviewed implementation and docs/flashing.md: identify actual board/build/slot/layout, fresh independently verified full-flash backup and recovery/write plan. No movement/pairing/calibration/limits/HA authorization.

Reuse web credentials for LCD controls/admin;120s inactivity. STOP grace for original unchanged continuing movement ends on observed stop or30s after expiry. One canonical lilygo-t-embed-cc1101 for CC1101/Plus; bundle web/controller/LCD and rollback together, configuration separate. Canonical web-image upload410. Browser owns backup/restore/manual files; no browser/LCD approval hybrid. Standalone updater trusts fixed verified GitHub HTTPS releases/digests.

Failed boot health at180s waits, then AUTOMATICALLY rolls back once movement/queued work finish and current state saves. Brief LCD waiting/scheduled message, no modal hiding target/STOP. New movement cancels pending restart, later idle automatically retries.

Unresolved: fonts/languages, Favourite stages versus old sequence, Relay separate/Euro tilt, visual-only Light, failed non-SEC full-section replacement, scan pause-all versus independent GPIO. Do not assume silence approves dependent flows.

## Workspace/status

Main clean e38fe16: controls/auth/STOP, settings/metadata CRUD/Reorder/GPIO, Health/Restart, bundled web, checked typed persistence/restore/resources/counters, stale browser/raw RF guards, Learn/primary counters, WiFi scan/passive diagnostics, scan-driver safety. Full parity incomplete.

Active isolated issue-25-standalone-install, codex/25-standalone-install, clean **04e6467**, based latest completed maine38. Shared worker verifies full PSRAM images, writes inactive slot, selects exact slot only after same SafeRestart final save/seals; cancellation/failed cleanup preserve ownership. Startup/TLS/continuous60s health, scoped public operations, standalone authenticated LCD updater, dev-compatible manual-image backend, automatic idle rollback+brief LCD messages implemented. No production canonical HTTP caller yet: raw legacy browser upload remains acceptance blocker. Do not claim branch independently reviewed or merged.

Rollback checkpoint04e completed14:19 PST. Actual policy/runtime/source/STOP-order tests and SDKs pass; 1163native pass+1existing skip,99JS pass. Logs /tmp/issue25-rollback-{qualified-final,sdk-qualified,ui,late-rx-mutant}.log and /tmp/issue25-updater-{native-all,js-all}.log. Inspected /tmp/issue25-rollback-ui/runtime-rollback-waiting-control.png. LateRX mutant fails; newly observed movement cancels restart BEFORE LCD input so STOP remains usable. Pinned rollback-enabled bootloader automatically aborts PENDING_VERIFY on second boot; ordinary checked restart uses this, no raw immediate SDK reboot.

Membership issue-25-group-membership-ui **2774835** clean, focused49+actualGFX+bothSDK pass; independent review unavailable, not merged. Scan issue-25-frequency-scan-service preserves partial service; driver increment already main. Historical #12 trees preserved.

## Active milestone I25-HTTP (started14:20, reassess14:40 PST)

Outcome: canonical browser upload stages bounded complete PSRAM image under existing manual lifecycle and hands exact file/security revision to shared installer through existing loop FIFO. No Update.begin/write/end, radio shutdown or boot selection in receive callbacks. Authenticated completion, timeout/disconnect/multiple-file/bounds/queue-failure cleanup tested; no request pointers in FIFO. Generic behavior unchanged in this increment. Next: inspect existing HTTP harness/browser handler; add failing staged-receive/handoff test, then implement. Browser honest progress polling follows. Reuse unchanged backend/rollback checks; focused receiver tests and affected SDKs only.

Remaining: browser receiver/progress, generic FS unmount/recovery, programming/repeater/remote/settled Favourite screens, full radio scan flow, unanswered choices, final matrix/docs/package checks and independent whole-change review.

## External blockers/tooling

All3 subagents hit usage limit untilNov10 22:36 (timezone unspecified); do not retry without capacity change or invent review. Human GitHub SidShaytay token invalid; stop GH operations, preserve unposted /tmp/issue25-current-unposted.md. SSH fetch works; no push/release/flash. Global Focus and efficiency rules installed ~/.config/agents/AGENTS.md; preserve Sid’s subsequent plain-language edit. Only ~/.agents/skills active.

PIO ~/.platformio/penv/bin/pio; native PATH ~/.cache/espsomfy-lilygo/native-toolbin; shared PLATFORMIO_LIBDEPS_DIR=<root>/issue-25-design/.pio/libdeps. Host CXX via podman exec -w <tree> rocm-7.2.3 bash. Serialize SDK/native per tree, never edit compiling sources. Git per-command commit.gpgsign=false (pinentry unavailable), no global change. Last user status ETA: several hours minimum; review blocked; next concrete checkpoint20minutes.

## I25-HTTP checkpoint — 14:27 PST

Canonical receive/loop handoff implemented and committed in installer (see latest log). Complete file stages under same owner/claim in full-slot PSRAM; no SDK writes/radio shutdown during receipt. Authenticated completion queues exact owner marker, loop invokes shared backend, normal queued disconnect does not cancel. Actual generic/bundled HTTP/FIFO tests cover14 receive/failure journeys plus prior restore/upload regressions, local-control queue regression passes. Both SDKs pass48.183/47.262s /tmp/issue25-browser-stage-sdk.log; host /tmp/issue25-browser-stage-{cases,queue}.log. Backend adoption positive already separately tested; full browser progress not yet qualified. Main/review/live state unchanged.

Next I25-HTTP-STATUS (reassess14:47): reserve a public operation ticket through same service before FIFO publication, retain through manual adoption, report matching authenticated cached status, and show honest browser progress/error/restart-scheduled without automatic replay or announcing installation from202. No new owner/request broker or browser/LCD hybrid. Add exact-ticket/security/stale/timeout/Close tests and affected SDKs; reuse unchanged receiver/rollback evidence.

## I25-HTTP-STATUS checkpoint — 14:35 PST

Installer **21f7e14** clean: same service reserves distinct public operation before FIFO publication, loop adoption preserves it; failed staging publishes refusal, new owner invalidates old observation. Authenticated cached /firmwareStatus validates exact canonical64-bit ticket/source. Browser distinguishes received/verifying/writing/final checks, bounds polling, Close ends observation only, lost/replaced status remains unconfirmed, no automatic replay or success from202. HTTP metadata bounded56 (+8 ticket), queue still80. Both SDKs pass14.879/46.797s, ELF contains real reserveManualInstall and runManualFirmware; all103JS pass. Real installer/controller includes reserved positive/failure tests, metadata+4000threadedclaims pass; HTTP/FIFO/status fixture passes with clearly labelled reservation/status boundary fakes. Logs /tmp/issue25-browser-ticket-{backend-final,http-final,js-all,sdk}.log. Reused unchanged1163native+1skip because changed production source is outside native filter. No main merge/review/live action. Prior stage checkpoint16b914f.

Next I25-UPDATER-PACKAGE (14:36, reassess14:56): validate one canonical bundled package from clean21f7e14, current image size/layout/identity and relevant update/packaging/board checks. Produce a concise installer review handoff; independent review remains unavailable, no merge/flash. Reuse unchanged native1163+1skip, latest103JS/worker/HTTP/SDK results. Do not expand unrelated runtime audits or invent hardware evidence; any actionable acceptance failure gets a focused reproducer.

## I25-UPDATER-PACKAGE checkpoint — 14:39 PST

Clean21f7e1496ed3ec89b8a1decf295248d9466d739f canonical package generated/validated46.904s at /tmp/issue25-updater-package-21f7e14/artifacts. Exact clean source asserted, firmware1934048 within0x330000, bundled web, no littlefs.bin. Actual newly built firmware passes same staged-image verifier (UPDATE_IMAGE_FIRMWARE). Package38+identity2+bundled10 and board/TLS/bootloader/release-script host checks pass /tmp/issue25-updater-package-host.log; build /tmp/issue25-updater-package.log; actual /tmp/issue25-updater-actual-image.log. Review handoff /tmp/issue25-updater-review.md prepared; no independent signoff, main merge, push, release, flash or controller action. Installer tree clean; no further package rebuild without a relevant source change.

# Issue25 execution record

Objective: complete standalone LCD parity, bundled LILYGO firmware/web and verified GitHub updates, with tests/docs, independent review and main integration. GitHub #25 is authoritative status; execution history is in issue-25.log. Sid explicitly says keep working between milestones; a status question is steering, not a reason to stop.

## Authority and decisions

Offline only. No controller, RF/motor/GPIO, live browser or HA operation performed. Flash authorized only after complete reviewed implementation and docs/flashing.md identity/slot/layout, fresh state/counters, verified full-flash backup and recovery checks. No movement authorization.

Reuse web credential;120s inactivity/shared throttle. STOP grace applies only to original continuing movement, ending at observed stop or30s after expiry. One CC1101/Plus build; web/controller/LCD bundled and rolled back together; config separate. Browser owns backup/restore/manual files, no hybrid approval. Fixed GitHub repository verified HTTPS/digests. Failed boot health at180s waits for idle+verified save then automatic rollback with brief LCD notice. Favourite uses explicit movement/physical-stop-observation/programming stages. English-only editing, preserve unchanged UTF-8. Independent GPIO works during RF scan. Retain relay tilt. Light auxiliary garage lamp remains open: web DOM-only toggle, no justified opcode; legacy unknown-string translation defaults to MY, never use it for Light.

## Workspace and review

Root sole record writer. Preserve all sibling worktrees. Clean main **e38fe16**; remote **fc5a294** freshly fetched and is ancestor. No pushes/releases/flashing. Three original reviewers hit quota untilNov10 22:36 (timezone unspecified); no retry without capacity change, no invented signoff or main merges.

Ready isolated branches (all require review/integration):

- issue-25-standalone-install **21f7e14**: updater/browser staging/status/rollback.1163native+1skip,103JS, actual worker/HTTP/UI/both SDK. Clean bundled package /tmp/issue25-updater-package-21f7e14/artifacts; review /tmp/issue25-updater-review.md.
- issue-25-group-membership-ui **2774835**:49focused/UI/both SDK.
- issue-25-advanced-remote **ebf4cf0**: explicit chords/Prog;1161native+1skip/UI/both SDK.
- issue-25-radio-scan-ui **fe27208**: owned browser/LCD scan, GPIO usable, no latent RF, review→draft only.1165native+1skip,104JS/UI/driver/queue/both SDK. #27 milestone6755396167; parent updated/readback verified.
- issue-25-favourite-workflow **84b9bb7**: COMPLETE LCD move via normal controls → observed stop → separately displayed default-Cancel Set/Clear → existing local FIFO/STOP. Exact observation revision catches fractional/ABA movement; expiry/change cancels; global idle and quiet RF rechecked. Set never clears, Clear never moves. No new queue/grant machinery. Backend a013950/be5b44e, observation15b6c0b.1165native+1skip /tmp/issue25-favourite-lcd-native-all.log; finalcontroller25 /tmp/issue25-favourite-controller-final.log; SDK8.768/9.742 /tmp/issue25-favourite-lcd-sdk-final.log. Full actual knob/GFX/auth/protected capture/Set/Clear/FIFO journey GREEN /tmp/issue25-favourite-ui-final.log; screenshots inspected /tmp/issue25-favourite-ui-final/. Adapter/queue GREEN /tmp/issue25-favourite-host-final.log; copied observation mutant fails /tmp/issue25-favourite-queue-mutant.log. Queue80B/pending632B/snapshot296B. #26 milestone6755903768 posted/readback verified. No rerun without relevant changes.
- issue-25-light-investigation **91c521e**: evidence/tests/docs only,100JS+24native, behavior unchanged.

## I25-FILESYSTEM-UPDATE ready for independent review

Clean isolated tree issue-25-filesystem-update, codex/25-filesystem-update **eebb167**, based main e38. Same GitUpdate manual owner/lifecycle covers shared mounted-reader/writer gate, loop preparation/recovery, exact prepared receiver, HTTP and browser choreography. Old receiveManualImage is firmware-only. Canonical separate Application route stays410. Generic preparation normal FIFO → checked current save → short seal publishing existing install pause/gate → reader drain → verified exact-partition unmount → exact byte/source/auth single file → complete authenticated request → remount(false) → CURRENT RAM/counter save → one bounded safe-restart attempt. Independent GPIO stays usable; retained install-pause RF refusal prevents latent movement. No new owner, layout, journal or LCD/browser hybrid.

Saved commits: reader0bc4f07, writer243ab04, lifecycleb40642f, RF FIFO1c49a3d, exact cleanup17a19e9, loop resources6c39e5a, receiver/restart9f98cea, HTTP/browser/docs eebb167. Exact-source attention retry never grants upload/restart; failed mount releases radio where possible. Fresh security rejection promptly retires only this adopted request; security history change promptly cancels prepared source. File-final alone never authorizes restart. Failed adoption cannot retire actual receiver. Metadata request has no upload-disconnect hook. No stale filesystem model/counter reload or formatting.

Evidence: full native **1168 pass+1 existing skip**, /tmp/issue25-fs-native-all.log; **105 JS pass**, /tmp/issue25-fs-browser-final.log; **41 actual owner/driver/receiver/HTTP scenarios**, /tmp/issue25-fs-revocation-green.log. Relevant SDK after final code9.542/5.457 /tmp/issue25-fs-candidate-qualified-sdk.log. Adjacent actual FIFO/control/restore/counter/socket/radio/pinned-FS/board suites GREEN /tmp/issue25-fs-integration-host-final.log; earlier unchanged evidence retained. Copied adoption-guard mutant fails overlap /tmp/issue25-adoption-mutant.log. Red/green source-revocation tests prove prompt cleanup. Generic buildfs succeeds /tmp/issue25-fs-image-build.log, exactly1572864 bytes matches spiffs partition; SHA25694b864d21d7be156be8b23b82186516684e6cb49a70b2d04c8885de6193ad7bc. Generated JS inspected for prepared flow. No image committed or installed.

Docs generic-filesystem-update.md and design guide rewritten around final flow and live-I/O audit. GitHub #29 body, parent#25 and #26 Favourite status updated/readback verified; component comments6757434478 and6757782582 verified. Review package /tmp/issue25-filesystem-review.md. Independent review/main integration blocked by known agent capacity untilNov10 22:36 (timezone unspecified), no invented signoff/retry without capacity change.

Integration must consolidate install and owned-scan admission guards with fe27208, preserving scan's accepted-work/driver restoration and GPIO guarantees. No combined acceptance claimed. Next independent LCD implementation: remaining #26 sensor simulation / explicit pair-program-jog stages from latest main in isolated worktree; do not stack on unreviewed branches. All product choices needed for these ordinary guarded flows are already accepted; Light stays unresolved/unsupported until justified opcode. Continue without milestone yield.

## Tools/status

GitHub SidShaytay confirmed: env -u GH_CONFIG_DIR /home/linuxbrew/.linuxbrew/bin/gh. Parent#25; #26 programming,#27 scan,#28 English/relay/Light/recovery,#29 genericFS,#30 integration; #12 updater. Normative reference comment6753845395. No current running tools/builds. Commit with per-command commit.gpgsign=false. SSH requires GIT_CONFIG_COUNT=0 and id_ed25519_2026_vega.

PIO ~/.platformio/penv/bin/pio; native PATH ~/.cache/espsomfy-lilygo/native-toolbin; shared PLATFORMIO_LIBDEPS_DIR=<root>/issue-25-design/.pio/libdeps. Host compiler via podman rocm-7.2.3. Serialize SDK/native per tree. Only ~/.agents/skills active. Reassess/change tactics after20min without a completed milestone. Final integration must reconcile updater extra System handle with160B budget.

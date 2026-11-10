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

## Active I25-FILESYSTEM-UPDATE (root)

Tree issue-25-filesystem-update, branch codex/25-filesystem-update, clean **172e0b0**, design only. Raw generic application upload currently writes mounted LittleFS; NOT fixed. Canonical LILYGO keeps410/bundled route.

Next bounded milestone: add mount I/O exclusion and track actual asynchronous static-file lifetimes, with failure/race tests. Reuse same ManualUpdateLifecycle/owner; no second upload broker. Current static handler opens files in canHandle; request disconnect runs before file destruction, so disconnect counters are not a safe drain proof. Plan a read-only FSImpl/FileImpl wrapper for web assets whose close/destructor closes the underlying file before releasing its gate lease. All static FS opens must acquire before touching LittleFS. Backup response is RAM, not an open FS response. Read pinned FS interfaces next and test the atomic gate before hooking wrapper.

Then implement loop-owned authenticated prepare→reader drain→checked save→verified unmount, exact-source upload and remount(false)/CURRENT RAM save recovery. LittleFS.end() is void; verify esp_littlefs_mounted(). No format/reload stale counters. Need settle writer deferral/control admission using existing services after tracing them, not add speculative control infrastructure. GenericFS plan is docs/generic-filesystem-update-design.md. New firmware/UI/source features must start from latest main; do not build on unreviewed feature branches.

## Tools/status

GitHub SidShaytay confirmed: env -u GH_CONFIG_DIR /home/linuxbrew/.linuxbrew/bin/gh. Parent#25; #26 programming,#27 scan,#28 English/relay/Light/recovery,#29 genericFS,#30 integration; #12 updater. Normative reference comment6753845395. No current running tools/builds. Commit with per-command commit.gpgsign=false. SSH requires GIT_CONFIG_COUNT=0 and id_ed25519_2026_vega.

PIO ~/.platformio/penv/bin/pio; native PATH ~/.cache/espsomfy-lilygo/native-toolbin; shared PLATFORMIO_LIBDEPS_DIR=<root>/issue-25-design/.pio/libdeps. Host compiler via podman rocm-7.2.3. Serialize SDK/native per tree. Only ~/.agents/skills active. Reassess/change tactics after20min without a completed milestone. Final integration must reconcile updater extra System handle with160B budget.

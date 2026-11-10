# Issue25 execution record

Objective: complete standalone LCD capability parity, bundled LILYGO web/firmware and verified GitHub updating. Acceptance requires full matrix/design, tests/docs, independent review and integration into main. Continue without yielding after each milestone unless a decision is needed. GitHub parent#25 is authoritative status; detailed prior evidence is in issue-25.log.

## Authority and decisions

Offline development only. No live controller, RF/motor, GPIO, HA or browser access performed. Flash authorized only after complete reviewed implementation and docs/flashing.md fresh backup/identity/slot/recovery checks; no movement authorization.

Reuse web credentials,120s inactivity/shared throttle. STOP grace ends observed original movement stop or30s after original expiry, never later movement. One CC1101/Plus build; bundle matching web/controller/LCD/rollback; configuration separate. Browser owns backup/restore/manual files; no hybrid approval flow. GitHub fixed-repository verified HTTPS/digests; no separate signature workflow. Failed health at180s waits automatically for idle+verified save before rollback; brief LCD notice preserves STOP. Favourite explicit movement/observed-stop/program stages; English-only editing, preserve unchanged UTF-8; independent GPIO usable during RF scan. Do not remove relay tilt. Light auxiliary garage lamp remains unsupported/unresolved: web toggles DOM only, no proved opcode; never route unknown Light through legacy translator (defaultsMY).

## Workspace/status

Root sole record writer; preserve all sibling worktrees. Clean authoritative main **e38fe16**, fetched remote main fc5a294 is ancestor. Nothing pushed/released/flashed. Ready isolated features below require independent review; original3 agents hit quota untilNov10 22:36 (timezone unspecified), no retry without capacity change. No signoff invented; no merges.

- issue-25-standalone-install **21f7e14**: updater/browser staging/status/rollback qualified1163native+1skip,103JS, actual worker/HTTP/runtime/bothSDK; exact clean bundled package /tmp/issue25-updater-package-21f7e14/artifacts; review /tmp/issue25-updater-review.md.
- issue-25-group-membership-ui **2774835**:49focused/fullUI/bothSDK.
- issue-25-advanced-remote **ebf4cf0**:45focused,1161native+1skip/fullUI/bothSDK; explicit chords/Prog.
- issue-25-filesystem-update **172e0b0**: design only, generic mounted filesystem upload gap NOT fixed.
- issue-25-light-investigation **91c521e**: tests/docs evidence,100JS+24native; no production behavior change.

## Active I25-SCAN-UI (root)

Tree issue-25-radio-scan-ui, branch codex/25-radio-scan-ui from e38, now clean **fe27208**. Commits588b admission,1a49365 owned service/protocol,3590511 browser,fe27208 LCD/docs. Actual driver/restore/FIFO/service pass /tmp/issue25-owned-scan-host-qualified.log. Fullnative1165pass+1existing skip /tmp/issue25-owned-scan-native-all.log; laterdisplayregression RED→GREEN /tmp/issue25-radio-overlap-{red,green}.log.104JS /tmp/issue25-radio-browser-final.log. Actual complete Runtime/GFX/auth/capture journey /tmp/issue25-radio-runtime-final.log. FinalSDKgeneric9.942/shared10.813 /tmp/issue25-owned-scan-sdk-final.log; lastJSassetcanonical17.633 /tmp/issue25-owned-scan-bundle-final.log. Sample/help overlap fixed using existingstackedRows and final scanning/review screenshots inspected /tmp/issue25-radio-ui-fixed/. Busy browserStart fixed with Close/explicitretry, reproducedRED. GPIOcontinues; noRFreplay; exactsource/token throughrestoration; Copy stagesdraftonly. Independentreviewstillblocked; no merge/liveactions. GitHub#27 milestone6755396167 posted/readback verified. No reruns without relevantchanges.

Active I25-FAVOURITE (root, next20min checkpoint): independent tree issue-25-favourite-workflow / codex/25-favourite-workflow from latestmain e38 (SSHremoteverifiedunchanged). **a013950** adds explicit program-here primitive, no implicit positioning; Setneverclears,Clearonlyatexactcurrentfavourite; alltilts/exactfractionalcoords,pendingTX/refusal/drop/save timing,toggle/drycontactrefusal.46nativebehaviourpass; copiedguardmutant fails3newcases /tmp/issue25-favourite-guard-mutant.log; bothSDK15.270/16.974 /tmp/issue25-favourite-stage-sdk.log. docs/favourite-programming-design.md records acceptedstagesandremainingLCDwork. Existingbrowserautomaticoperationunchanged.

CurrentUNCOMMITTED36line shared-request/wrapper adapter adds separate set_favorite_here/clear_favorite_here throughShadeRequests/SomfyShadeOps; preservesRFclassification/marker size,no wireopcode orUIyet. Focused81behaviour+requestpass /tmp/issue25-favourite-shared-final.log. Fullnative→bothSDK executing **session56870** /tmp/issue25-favourite-native-all.log and /tmp/issue25-favourite-shared-sdk.log; do noteditcompiling sources. Next: check tests/builds, commit adapter, update#26milestone; then authenticated LCDmove→humanobservedstop→defaultCancelprogram with exactcontext/FIFO/RF-awaremarker. Do not use old setMyPosition in finalprogramconfirmation: it auto-moves/programs. Need observedsnapshotfloatingpositions andfreshness beyond rawconfiguration revisions. Reconcile same small usesRadio hook fromunreviewedremote branch ratherthanstackthatfeature; never silentlypublishRFasnonRF. Finalintegration must reconcile updaterextra Systemhandle with160Bbudget. No newuserdecisionneedednow.

## GitHub/tools

Parent#25 concise plan; real#26 programming,#27 scan,#28 English/relay/Light/recovery,#29 genericFS,#30 integration; reuse#12 updater. Full normative reference comment6753845395. Last#27 admission comment6754778097. Account SidShaytay confirmed; use env -u GH_CONFIG_DIR /home/linuxbrew/.linuxbrew/bin/gh under global exception. SSH network requires GIT_CONFIG_COUNT=0 and explicit id_ed25519_2026_vega. Commits per-command commit.gpgsign=false.

PIO ~/.platformio/penv/bin/pio; native PATH ~/.cache/espsomfy-lilygo/native-toolbin; shared PLATFORMIO_LIBDEPS_DIR=<root>/issue-25-design/.pio/libdeps. Host compiler via podman rocm-7.2.3. Serialize builds per tree. Reuse unchanged tests; reassess if20min without milestone. Only ~/.agents/skills active.

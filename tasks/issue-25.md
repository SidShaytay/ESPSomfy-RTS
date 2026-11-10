# Issue #25 execution record

Objective: finish standalone LCD parity, bundled web/firmware and verified GitHub updater, tests/docs, independent review and main integration. GitHub #25 is authoritative; detailed evidence/history is in [issue-25.log](issue-25.log). Root is the sole record writer. Continue between milestones; status questions are steering, not cancellation. Reassess after20min without a completed milestone; prioritize requested behavior and reuse unchanged checks.

## Authority and scope

Offline development only: no controller/serial/live-browser/RF/motor/GPIO/HA access performed. Flash authorized only after completed reviewed implementation and docs/flashing.md actual board/build/features/slot/layout, fresh state/counters, independently verified full-flash backup and exact recovery/write/readback plan. No movement/pairing/calibration/limits or HA permission.

Accepted: reuse web credential,120s inactivity/shared throttle; original continuing-movement STOP grace until stop or30s after expiry. One CC1101/Plus build, bundled matching web/controller/LCD and rollback; configuration separate. Browser owns backup/restore/manual files, no hybrid. Fixed repository verified HTTPS/digests. Failed boot health at180s waits for idle+verified save, brief LCD explanation, automatic rollback. Favourite explicit move→observed stop→separate program confirmation. English-only editor, preserve existing UTF8. Independent GPIO during RF scan. Retain relay tilt modes.

Latest explicit answers: **Light unavailable**, preserve lamp configuration, never invent an opcode/MY fallback or lamp state. **Existing settings recovery; no new whole-section LCD repair wizard**. Keep unreadable sections unavailable and readable-field Replace; no security/default reset. Normal flashing preserves NVS; backups omit WiFi/MQTT credentials. No pending user choice for either.

## Workspace and review

Preserve all sibling worktrees. Clean authoritative local main **e38fe16**; fetched remote main **fc5a294** is ancestor. No pushes/releases/flashes. All candidates below are isolated, based on main, unpushed and NOT integrated. Main defines completion. Required independent review precedes integration; do not silently combine unreviewed branches. Agent inventory confirms3 reviewers errored usage limit untilNov10 22:36 (timezone unspecified); no capacity-change evidence, retry or signoff. Continue independent acceptance work.

| Tree suffix (`issue-25-…`) | Clean candidate | Qualification / review package |
|---|---|---|
| standalone-install |21f7e14|1163native+skip,103JS, actual worker/HTTP/UI/SDK; package /tmp/issue25-updater-package-21f7e14/artifacts; /tmp/issue25-updater-review.md |
| group-membership-ui |2774835|49focused/UI/SDK; controller-list metadata, no motor pairing |
| advanced-remote |ebf4cf0|1161native+skip/UI/SDK; explicit chords/Prog |
| radio-scan-ui |fe27208|1165native+skip,104JS/UI/driver/FIFO/SDK; owned scan, GPIO usable, no latent RF |
| favourite-workflow |84b9bb7|1165native+skip/UI/SDK; complete Set/Clear journey, exact observation including fractional/ABA; /tmp/issue25-favourite-ui-final.log |
| filesystem-update |eebb167|1168native+skip,105JS,41actual owner/driver/receiver/HTTP cases/UI/SDK/buildfs; /tmp/issue25-filesystem-review.md |
| sensor-simulation |b6d1f42|1161validated native+skip across runs, actual UI/FIFO/runtime/SDK; /tmp/issue25-sensor-review.md |
| group-step-forwarding |64461f5|102JS,66focused native +58final protocol/domain, bundled SDK/generatedJS; /tmp/issue25-group-step-review.md |
| guided-programming |0dbfe3d|1165native+skip plus finalruntime16; actual knob/GFX/auth/capture/FIFO,4previews,SDK14.372/16.265; /tmp/issue25-programming-review.md |
| group-programming |03160d4|1166native+skip before final RF-tag fix; finalruntime18 and actual UI/FIFO/SDK4.232/9.259 after fix; /tmp/issue25-group-programming-review.md |
| light-investigation |91c521e|Evidence/tests/docs only,100JS+24native; no behavior change |

Programming guides: defaultCancel, typed Prog only, exact fresh auth/group/device/radio/owner/idle/5s witnesses. Device guide requires observed learn mode and intended physical result before separate paired-marker save. Group guide separately reviews deviceProg40 and broadcast groupProg1, or observed external learn-mode entry; warns other motors may be affected; separate result verification/list-only save. No inferred success, compensating RF or replay on file retry. Group final logs /tmp/issue25-group-programming-{runtime-marker,ui-qualified,sdk-final}.log. Four inspected PNGs under its .pio/ui-group-programming-preview. RF marker assertion caught retained unconditional false overwrite; fixed; other candidates already correct. #26 body/comment6759536487 verified.

Filesystem candidate uses SAME manual GitUpdate lifecycle/owner, shared reader/writer gate, exact-source prepared upload and loop-only verified unmount/remount(false)/CURRENT RAM+counter save; no stale reload/formatting. Canonical separate Application route410. Retained install RF refusal must consolidate with owned-scan admission, preserving GPIO/no-loss behavior. Generic image verified1572864bytes. Common Advanced menu/Mode/Backend/usesRadio additions across candidates need deliberate reviewed reconciliation. No combined acceptance claimed.

## Active [-] I25-LIGHT-UNAVAILABLE

Tree issue-25-light-unavailable / codex/25-light-unavailable from latest main e38; uncommitted adapter/renderer/test/docs edits. Objective: configured has-light device shows noninteractive Light unavailable; no action/opcode/mask/config/counter changes, ordinary controls/STOP preserved. Settings-load-trust and matrix wording updated to accepted recovery scope. Adapter regression RED missingfield /tmp/issue25-light-unavailable-red.log, implementation reads shade.light as capability only. Full UI/GFX/adapter/auth/capture/FIFO harness currently running /tmp/issue25-light-unavailable-green.log (session10733); source edits complete, no SDK/native running. Next: verify harness completion; inspect control-light-unavailable and control-tilt-light-unavailable PNGs in .pio/ui-light-preview, both SDK, focused relevant native if needed, docs/review package/commit/GitHub. Do not stack on other candidates.

## Remaining [ ] I25-CAPABILITY-RECONCILE

After Light milestone, audit complete matrix against main/candidate evidence; publish exact gaps and integration sequence in #28/#30. English criterion already verified main:19focused /tmp/issue25-english-main-verification.log. Relay audit /tmp/issue25-relay-tilt-main-verification.log GREEN actual fake driver all5modes/bothpolarities. GP_Relay physicalMask onlyUp/Down; tiltDirection fallback integrated/tiltonly, separate/Euro centrally refused. Modes remain configured. Do not invent extra pins/electrical semantics or claim unsupported mapping complete. No blanket parity deferral approved.

GitHub #25 parent, #26 programming, #27 scan, #28 capability, #29 filesystem, #30 integration; #12 updater. Normative acceptance comment6753845395. Latest parent/#28 accepted-scope bodies written/readback verified (/tmp/issue25-scope-status.md,/tmp/issue28-scope-status.md); Light implementation box stillopen. Parent group-candidate row and #30 concrete integration order still need reconciliation.

## Tool essentials

Human exception: env -u GH_CONFIG_DIR /home/linuxbrew/.linuxbrew/bin/gh; SidShaytay confirmed. SSH fetch/push only explicitrepo via GIT_CONFIG_COUNT=0, GIT_SSH_COMMAND ssh -i /var/home/sid/.ssh/id_ed25519_2026_vega -o IdentitiesOnly=yes -o BatchMode=yes. No pushes yet. Commit per-command commit.gpgsign=false (pinentry unavailable).

PIO /var/home/sid/.platformio/penv/bin/pio; native PATH /var/home/sid/.cache/espsomfy-lilygo/native-toolbin:$PATH; shared PLATFORMIO_LIBDEPS_DIR=<root>/issue-25-design/.pio/libdeps. Serialize SDK/native per tree, never edit compiling sources. Hostcompiler podman exec -w <tree> rocm-7.2.3. preview-local-ui.sh needs GFX_DIR=<shared>/lilygo-t-embed-cc1101/Adafruit GFX Library and ARDUINOJSON_DIR=<shared>/esp32s3/ArduinoJson/src. Standalone host scripts use ARDUINOJSON_INCLUDE; FS harness ARDUINO_FS_DIR=/var/home/sid/.platformio/packages/framework-arduinoespressif32/libraries/FS/src. Only active ~/.agents/skills, no source-library scans. Detailed past logs/history need not be loaded for routine resumption.

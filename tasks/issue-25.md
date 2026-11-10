# Issue #25: LCD implementation and qualification

The goal is full agreed LCD, button and rotary functionality, including standalone GitHub updates. Most functionality is integrated. The approved navigation is now integrated in main0d6a7f5; the newly requested30-minute installer/retry behavior is in development. Next deployment, physical acceptance and combined-load qualification remain incomplete. GitHub issues #25, #12 and #30 are open and contain the current public status.

## Next action — live ROM capture

Root's ONE combined full16 read is RUNNING as session27318 in `~/.local/state/espsomfy-lilygo/phase480-preflight-75ewxueo`. It was last43% complete. Do not restart/read partial raw, restore logger or boot until terminal0 and NEW independent raw proof. Screen maintenance was announced; fresh current473 before-capture and backup passed; reviewed operators were activated only after exact current473 confirmed boot and candidate package readiness. Logger is paused and exclusive port proved. No RF or firmware writes.

Agent phase2_independent_review prepared peer480-offline decoder but must wait for explicit complete notification. It will prove BOTH post473 acceptance and next480 recovery, including OTA21VALID/currentapp0 exact473, ENTIREretainedapp1 old471, all16NVS/EVERYLittleFS/effective counters. Root's guarded normal473 helper runs only after that new proof binds current raw/self/plan/before/passive hashes. Preserve old pending markers.

The full navigation and30-minute/retry implementation is complete and integrated. Release candidate v4.8.0 tag/source54fe4b57 (immutable) is built and independently audited. Mainca5e760 adds only two guide wording corrections, with identical runtime code. Canonical SDK96545 TERMINAL0 PASS50.430s; generic esp32dev43745 TERMINAL0 PASS39.974s; native53325 TERMINAL0 1291passes/1optional skip; web118passes and version scriptpasses. Logs /tmp/i25-v480-{sdk,generic,native,js,version}.log. Full actual Runtime/GFX/auth/capture/FIFO,47 navigation native,12updateforms,43installer/30controller/2developerfile,69worker+threadedownership and mutation tests passed in prior named logs.

Candidate package: `issue-25-engineering-v480/build/v480-tagged-package/artifacts`; two assets in build/v480-release-assets. FW1,991,808B SHAa7d9402578cb75dc1323f5a9aeb888a1190092561e099a70d236c3527b985b0e; ELF c24034609b2517cdb0b9d3f543a22d671da83ef78f485095fe0dd794b88eb4ed; JSON067706cc311ec5767ecb4a50d02866b5b595c6ca78c86876521b59891b494df2. Layout/bootloader/profile match473. Private package audit approved. Not published or installed yet. After raw proof and normal473 restore/statecomparison: publish engineering480, prepare exact firmware-only write plan/observer, then authorized flash. No new permission needed for existing stage authorization.

A temporary docs worktree was accidentally nested under the release checkout; it was moved to its proper sibling path with all work preserved. Tagged source is clean again and release assets now package successfully. No retag/rebuild or firmware change; mainca5e760 corrects two old guide sentences outside immutable tag54fe. Preserve both source identities.

## Completed work

The board last verified runs **4.7.1**, source `5e1657d`, app1, OTA sequence 20 VALID. Exact firmware identity, SDK health confirmation, full-flash verification, normal reboot and settings/entities/counter comparisons passed before the cancelled attempt.

The GitHub 4.7.2 firmware download completed with HTTP 200 and successful certificate verification. The attempt then cancelled; Sid read “Update cancelled; controls resume after cleanup”. The board still reports 4.7.1, with no new-image boot or SDK confirmation observed. App0 contents and boot selection have not been independently read since that attempt. Never claim app0 still contains 4.6.1.

Engineering **4.7.3** is published at https://github.com/SidShaytay/ESPSomfy-RTS/releases/tag/v4.7.3 and now booted and SDK-confirmed through Sid's actual LCD GitHub upgrade. Raw postread and physical qualification remain pending. Immutable tag source: `1fb7abc1552b52e21fd1e1892930e77d43ca2ff9`. It includes approved automatic release review with Cancel selected, hides empty release information, shows “Checking GitHub…” and keeps update progress/results visible while dimmed. Security expiry remains active. The old 21-second idle cancellation was reproduced; regression tests and independent review passed. The canonical SDK build passed in 48.773 seconds. Public firmware and verification JSON match the independently reviewed package.

Firmware SHA256: `0ff41528484210ce640e6ce579db2cc1f2fa779933d72522c17e40a1ae97d995`. ELF SHA256: `165e6d5d4448c9ab7fbecc708af6c7103bcdf38c7e8b17038ae234e503979f5e`. The package is in `issue-25-engineering-v473/build/v473-tagged-package/artifacts`; publication assets and readback are beside it. Main is `75c85f1`, including an additional encoder acknowledgement regression and corrected qualification/update documentation. No repeated tests are needed for those unchanged results.

## Recovery and observers

Private state root: `~/.local/state/espsomfy-lilygo`.
- Previous verified deployment: `phase471-preflight-00gygl2c`.
- Cancelled attempt and verified pre-attempt full 16 MiB backup: `phase472-preflight-cgcoagua`. Preserve its pending markers and all raw evidence.
- New recovery preparation: `phase473-preflight-llv_kko3`; fresh read-only capture and portable backup are saved. Its new read/reset plan and boot helper have conditional independent approval, **execution_approved=false**. After Sid's fresh Check-row answer, record and bind that evidence before any reset. That pre-update plan is no longer executable after the successful473 update. The new combined post473/next-release plan must prove actualOTA21VALID, exact currentapp0 and retainedapp1 plus allNVS/files/counters before normalboot. No root recovery operation has executed. Sid began the LCD GitHub update instead, so the prepared pre-update ROM read/bootback plan is obsolete for the current pending image. Do not execute it. New post-install operators must target exact473 app0, retained471 app1 and actual new OTA state after SDK confirmation.

Existing passive USB logger remains attached. Read only `logs/usb.log`; do not open serial. File observer 32212 and health observer 73354 were confirmed live at 19:54:57 UTC on November 10. The anchored log still contains no new boot record after the attempt; old pending guards are preserved. Original observer 3382 completed successfully; original boot observer 82682 ended inconclusively. No successful new-image confirmation file exists. A timeout never authorizes reset or retry. Latest passive sample at 19:47:29 UTC: internal free 126,756 B, PSRAM free 8,239,600 B, TLS allocations released, zero allocation failures. These samples do not prove complete workload qualification.

## Authority and pending design

Root alone operates the board. Flashing and engineering publication remain authorized. No HA changes. RF permission expires November 11 at 04:24:29 UTC; announce before and after every RF session. No RF session has started. Do not add pairing, calibration or motor-limit tests.

Use visible engineering warnings and normal release eligibility; no alpha channel or temporary software blocks. Sid gave final navigation approval on November 10. Sid proposed quick entries, Browse all, and This controller containing Configure, Maintenance, Status (existing wake screen), About and Diagnostics. The complete proposal is saved in `issue-25-navigation-proposal/docs/design/lcd-navigation-proposal.md`, commit `75c85f1`, independently reviewed and pushed to main as discussion documentation. It proposes preserving current wake behavior and existing privacy requirements. Sid has now explicitly approved this proposal; implement it without changing current wake/privacy requirements.

Follow the replacement AGENTS.md writing rules: natural sentences, familiar words and clear outcomes. Human GitHub exception remains: verify SidShaytay using `env -u GH_CONFIG_DIR /home/linuxbrew/.linuxbrew/bin/gh`. Git network commands need `GIT_CONFIG_COUNT=0`, SSH URL and `SSH_AUTH_SOCK=/run/user/1000/gcr/ssh`. Preserve all worktrees, tags and private recovery files. Older decisions and exact historical operator details are archived in `issue-25.log`; read only when needed.

## Current observation

The blocked audit is historical and resolved. Sid confirmed the required Check row, approved navigation and completed the LCD update to 4.7.3. Passive exact-image boot is proved; SDK confirmation is now proved; independent fresh state/counter/raw checks remain. Old evidence and pending guards are preserved. New phase473 pending/observation markers record that the write was user-triggered, not a root upload. The observer has independent review and never opens serial.

Combined post473/next-release recovery plan is prepared in `phase480-preflight-75ewxueo` (pointer PHASE480_PREFLIGHT_CURRENT). Root independently approved the exact plan/helper; execution remains gated on the next package being ready, fresh current473 capture and announced/exclusive maintenance. ONE full16 read will serve both purposes. Do not execute stale pre473 operators.

Navigation documentation is now integrated/pushed as main0d6a7f5:20 current guides map the new paths and explicitly distinguish installed473 from upcoming source. Historical design/hardware evidence stays unchanged. Root reviewed the focused diff and actual three menu previews; author link/diff checks passed. Installer source/tests are being developed in separate trees; no new firmware has been published or flashed.

Installer continuation slice d19bc71 in `issue-25-install-continuation` is independently source-approved: fixed30min job only, normal menu locks, locked detach/fresh-unlock exact-operation resume/cancel, credential/config/model/clock revocation.12 native forms and actual installer/controller harness passed. Actual Runtime acceptance is in progress; retry is a separate next commit. Root flagged deadline cause must report timeout, not ambiguous Cancel, and supplied official GitHub rate-limit delay requirements. Normative #25 comment6753845395 now contains the explicit30-minute/retry requirement as pending; readback verified.

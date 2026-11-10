# Issue #25: LCD implementation and qualification

The goal is full agreed LCD, button and rotary functionality, including standalone GitHub updates. Software implementation is integrated; deployment of the latest fixes, physical acceptance and combined-load qualification remain incomplete. GitHub issues #25, #12 and #30 are open and contain the current public status.

## Next action

Sid reports Check GitHub release available, approved the complete navigation proposal and performed a 4.7.3 LCD GitHub upgrade. He now sees 4.7.3. The passive USB log shows exact reviewed app0 ELF boot at 20:12:12 UTC on November 10. Observer33937 completed successfully: exact boot20:12:12 UTC, SDK health confirmed20:13:18 UTC and network4.7.3. Independent passive/state audit is underway; no reset, USB change or retry. Old 4.7.2 observer 32212 ended inconclusively; health observer 73354 remains live.

Implement the approved navigation in parallel. Agent phase2_status owns production navigation and focused native tests. Agent checkpoint_review owns actual Runtime/encoder tests in a separate tree. Both start from main75c85f1; no device actions. Independent review and SDK validation precede integration/release. The next milestone is an independently verified 4.7.3 GitHub installation plus a tested navigation implementation. Sid raised the close two-minute session timeout: proposed change lets only an explicitly confirmed install continue within the existing180-second operation bound after menus lock. Credential/configuration invalidation remains. A new async decision is pending; do not implement that security change before the reply. Navigation implementation proceeds independently.

## Completed work

The board last verified runs **4.7.1**, source `5e1657d`, app1, OTA sequence 20 VALID. Exact firmware identity, SDK health confirmation, full-flash verification, normal reboot and settings/entities/counter comparisons passed before the cancelled attempt.

The GitHub 4.7.2 firmware download completed with HTTP 200 and successful certificate verification. The attempt then cancelled; Sid read “Update cancelled; controls resume after cleanup”. The board still reports 4.7.1, with no new-image boot or SDK confirmation observed. App0 contents and boot selection have not been independently read since that attempt. Never claim app0 still contains 4.6.1.

Engineering **4.7.3** is published at https://github.com/SidShaytay/ESPSomfy-RTS/releases/tag/v4.7.3 and now booted and SDK-confirmed through Sid's actual LCD GitHub upgrade. Raw postread/state qualification remains pending. Immutable tag source: `1fb7abc1552b52e21fd1e1892930e77d43ca2ff9`. It includes approved automatic release review with Cancel selected, hides empty release information, shows “Checking GitHub…” and keeps update progress/results visible while dimmed. Security expiry remains active. The old 21-second idle cancellation was reproduced; regression tests and independent review passed. The canonical SDK build passed in 48.773 seconds. Public firmware and verification JSON match the independently reviewed package.

Firmware SHA256: `0ff41528484210ce640e6ce579db2cc1f2fa779933d72522c17e40a1ae97d995`. ELF SHA256: `165e6d5d4448c9ab7fbecc708af6c7103bcdf38c7e8b17038ae234e503979f5e`. The package is in `issue-25-engineering-v473/build/v473-tagged-package/artifacts`; publication assets and readback are beside it. Main is `75c85f1`, including an additional encoder acknowledgement regression and corrected qualification/update documentation. No repeated tests are needed for those unchanged results.

## Recovery and observers

Private state root: `~/.local/state/espsomfy-lilygo`.
- Previous verified deployment: `phase471-preflight-00gygl2c`.
- Cancelled attempt and verified pre-attempt full 16 MiB backup: `phase472-preflight-cgcoagua`. Preserve its pending markers and all raw evidence.
- New recovery preparation: `phase473-preflight-llv_kko3`; fresh read-only capture and portable backup are saved. Its new read/reset plan and boot helper have conditional independent approval, **execution_approved=false**. After Sid's fresh Check-row answer, record and bind that evidence before any reset. A new raw proof must establish unchanged OTA sectors, exact app1 preservation, all NVS/files/counters and actual app0 differences before booting. No root recovery operation has executed. Sid began the LCD GitHub update instead, so the prepared pre-update ROM read/bootback plan is obsolete for the current pending image. Do not execute it. New post-install operators must target exact473 app0, retained471 app1 and actual new OTA state after SDK confirmation.

Existing passive USB logger remains attached. Read only `logs/usb.log`; do not open serial. File observer 32212 and health observer 73354 were confirmed live at 19:54:57 UTC on November 10. The anchored log still contains no new boot record after the attempt; old pending guards are preserved. Original observer 3382 completed successfully; original boot observer 82682 ended inconclusively. No successful new-image confirmation file exists. A timeout never authorizes reset or retry. Latest passive sample at 19:47:29 UTC: internal free 126,756 B, PSRAM free 8,239,600 B, TLS allocations released, zero allocation failures. These samples do not prove complete workload qualification.

## Authority and pending design

Root alone operates the board. Flashing and engineering publication remain authorized. No HA changes. RF permission expires November 11 at 04:24:29 UTC; announce before and after every RF session. No RF session has started. Do not add pairing, calibration or motor-limit tests.

Use visible engineering warnings and normal release eligibility; no alpha channel or temporary software blocks. Sid gave final navigation approval on November 10. Sid proposed quick entries, Browse all, and This controller containing Configure, Maintenance, Status (existing wake screen), About and Diagnostics. The complete proposal is saved in `issue-25-navigation-proposal/docs/design/lcd-navigation-proposal.md`, commit `75c85f1`, independently reviewed and pushed to main as discussion documentation. It proposes preserving current wake behavior and existing privacy requirements. Sid has now explicitly approved this proposal; implement it without changing current wake/privacy requirements.

Follow the replacement AGENTS.md writing rules: natural sentences, familiar words and clear outcomes. Human GitHub exception remains: verify SidShaytay using `env -u GH_CONFIG_DIR /home/linuxbrew/.linuxbrew/bin/gh`. Git network commands need `GIT_CONFIG_COUNT=0`, SSH URL and `SSH_AUTH_SOCK=/run/user/1000/gcr/ssh`. Preserve all worktrees, tags and private recovery files. Older decisions and exact historical operator details are archived in `issue-25.log`; read only when needed.

## Current observation

The blocked audit is historical and resolved. Sid confirmed the required Check row, approved navigation and completed the LCD update to 4.7.3. Passive exact-image boot is proved; SDK confirmation is now proved; independent fresh state/counter/raw checks remain. Old evidence and pending guards are preserved. New phase473 pending/observation markers record that the write was user-triggered, not a root upload. The observer has independent review and never opens serial.

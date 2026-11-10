# Issue #25: LCD implementation and qualification

The goal is full agreed LCD, button and rotary functionality, including standalone GitHub updates. Software implementation is integrated; deployment of the latest fixes, physical acceptance and combined-load qualification remain incomplete. GitHub issues #25, #12 and #30 are open and contain the current public status.

## Next action

Wait for Sid's answer to the pending question: without retrying, does the GitHub update screen offer **Check GitHub release**, or only **Back: System**? A fresh Check row proves the previous operation is no longer busy. This is required before resetting the controller. The cancelled message alone is insufficient. Do not retry an update or change the USB connection while waiting.

The next completed milestone is deploying and verifying engineering 4.7.3, then finishing the real LCD GitHub upgrade and physical qualification. Continue independent work without changing the broader navigation until Sid gives final approval.

## Completed work

The board last verified runs **4.7.1**, source `5e1657d`, app1, OTA sequence 20 VALID. Exact firmware identity, SDK health confirmation, full-flash verification, normal reboot and settings/entities/counter comparisons passed before the cancelled attempt.

The GitHub 4.7.2 firmware download completed with HTTP 200 and successful certificate verification. The attempt then cancelled; Sid read “Update cancelled; controls resume after cleanup”. The board still reports 4.7.1, with no new-image boot or SDK confirmation observed. App0 contents and boot selection have not been independently read since that attempt. Never claim app0 still contains 4.6.1.

Engineering **4.7.3** is published at https://github.com/SidShaytay/ESPSomfy-RTS/releases/tag/v4.7.3 but is not installed. Immutable tag source: `1fb7abc1552b52e21fd1e1892930e77d43ca2ff9`. It includes approved automatic release review with Cancel selected, hides empty release information, shows “Checking GitHub…” and keeps update progress/results visible while dimmed. Security expiry remains active. The old 21-second idle cancellation was reproduced; regression tests and independent review passed. The canonical SDK build passed in 48.773 seconds. Public firmware and verification JSON match the independently reviewed package.

Firmware SHA256: `0ff41528484210ce640e6ce579db2cc1f2fa779933d72522c17e40a1ae97d995`. ELF SHA256: `165e6d5d4448c9ab7fbecc708af6c7103bcdf38c7e8b17038ae234e503979f5e`. The package is in `issue-25-engineering-v473/build/v473-tagged-package/artifacts`; publication assets and readback are beside it. Main is `8b746f2`, including an additional encoder acknowledgement regression and corrected qualification/update documentation. No repeated tests are needed for those unchanged results.

## Recovery and observers

Private state root: `~/.local/state/espsomfy-lilygo`.
- Previous verified deployment: `phase471-preflight-00gygl2c`.
- Cancelled attempt and verified pre-attempt full 16 MiB backup: `phase472-preflight-cgcoagua`. Preserve its pending markers and all raw evidence.
- New recovery preparation: `phase473-preflight-llv_kko3`; fresh read-only capture and portable backup are saved. Its new read/reset plan and boot helper have conditional independent approval, **execution_approved=false**. After Sid's fresh Check-row answer, record and bind that evidence before any reset. A new raw proof must establish unchanged OTA sectors, exact app1 preservation, all NVS/files/counters and actual app0 differences before booting. No recovery operation has executed.

Existing passive USB logger remains attached. Read only `logs/usb.log`; do not open serial. File observer 32212 and health observer 73354 were confirmed live this turn. Original observer 3382 completed successfully; original boot observer 82682 ended inconclusively. No successful new-image confirmation file exists. A timeout never authorizes reset or retry. Latest passive sample at 19:47:29 UTC: internal free 126,756 B, PSRAM free 8,239,600 B, TLS allocations released, zero allocation failures. These samples do not prove complete workload qualification.

## Authority and pending design

Root alone operates the board. Flashing and engineering publication remain authorized. No HA changes. RF permission expires November 11 at 04:24:29 UTC; announce before and after every RF session. No RF session has started. Do not add pairing, calibration or motor-limit tests.

Use visible engineering warnings and normal release eligibility; no alpha channel or temporary software blocks. Broader navigation requires final approval. Sid proposed quick entries, Browse all, and This controller containing Configure, Maintenance, Status (existing wake screen), About and Diagnostics. The complete proposal is saved in `issue-25-navigation-proposal/docs/design/lcd-navigation-proposal.md`, commit `75c85f1`, with independent review pending. It proposes preserving current wake behavior and existing privacy requirements. Do not implement it before final approval.

Follow the replacement AGENTS.md writing rules: natural sentences, familiar words and clear outcomes. Human GitHub exception remains: verify SidShaytay using `env -u GH_CONFIG_DIR /home/linuxbrew/.linuxbrew/bin/gh`. Git network commands need `GIT_CONFIG_COUNT=0`, SSH URL and `SSH_AUTH_SOCK=/run/user/1000/gcr/ssh`. Preserve all worktrees, tags and private recovery files. Older decisions and exact historical operator details are archived in `issue-25.log`; read only when needed.

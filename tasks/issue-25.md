# Issue #25 — active; GitHub installation and physical qualification incomplete

Objective: complete LCD parity and agreed standalone workflows with tests, documentation, independent review and phased engineering builds. Acceptance still requires a real GitHub installation, post-write state/counter verification and remaining physical qualification. Preserve all existing work. GitHub issues #25/#12/#30 are authoritative public status.

## Current milestone

[-] Deliver approved updater fixes as engineering 4.7.3. Next: finish SDK/package review, resolve cancelled-attempt activation uncertainty, then prepare the exact authorized deployment/recovery plan. Broader navigation is FROZEN pending final approval.

Installed: 4.7.1, source5e1657d, app1 previously VALID/sequence20. Sid reports EXACT “Update cancelled; controls resume after cleanup”. HTTPversion and passive logs still471; no new boot/SDK confirmation. Actual GitHub firmware HTTPS download DID complete (http200/TLS flags0/cert3). Installation did NOT complete. Inactive app0 contents/boot selection remain UNKNOWN after cancellation: no retry/reset/USB handoff or old bootback helper until resolved. Pending guards remain intact.

Root integrated independently reviewed60bdcf8 (hide empty release row; indeterminate Checking GitHub; own successful check opens review with Cancel selected) and0bfaf48 (DIM-only update context retention, no idle return/cancellation). Reproduced old actualRuntime failure at21s; full UI/native9 tests green, independent retention mutation killed. Security120s expiry/cancellation/privacy/pool cleanup remain active; no fake activity or extended authority. Author checkpoint_review adding explicit expired Back-row Runtime press test; no identified source bug.

4.7.3 isolated worktree issue-25-engineering-v473 at1fb7abc, local immutabletagv4.7.3. Version/cache4files+docs only atop0bf. SDK session73707, log /tmp/i25-v473-sdk.log, package build/v473-tagged-package/artifacts. Peer phase2_independent_review assigned package/version audit. Not published/deployed. Main currently0bfaf48. Last public main/tagv4.7.2 e43cc14; never retag/overwrite.

## Private evidence / live observers

State root ~/.local/state/espsomfy-lilygo. Current p472=phase472-preflight-cgcoagua (PHASE472_PREFLIGHT_CURRENT). Full16 configured-16mb.bin and independent-full-backup-verification.json approve both471postread and pre472recovery, all16NVS/ALLLittleFS/entities/counters. p471=phase471-preflight-00gygl2c; final-deployment-verification.json, LATEST_VERIFIED_DEPLOYMENT/ISSUE25_FINAL_DEPLOYMENT identify471. Preserve raw backups privately.

p472/new-image-may-be-pending.json and firmware-write-started.json created BEFORE userInstall; not themselves proof of writes. Existing passive O_RDONLY USBlogger remains attached; no root serialoperations since guards. Read its files only.

Original bootwatch82682 TERMINAL1 inconclusive; extended reviewed anchored watch32212 last LIVE (watch-v472-extended.py SHA10695e72…). Healthfileobserver73354 lastLIVE, extended60min; original3382 needs actual poll. ExistingUSB log ~/.local/state/espsomfy-lilygo/logs/usb.log. Never infer failed install/safe reset from observer timeout. No passive-confirmation-verification.json yet. Future p472/postwrite operators require exact472app0ELF→SDK proof and new independent raw proof; NOT applicable to cancelled471.

Download sampled extrema: minInternal109640, minLargest65524, maxPSRAM2167520, workerStack4068, TLSpeak51545/internal12376, allocationfail0, cleanuprequested0. Samples are not complete maxima. Target472 fw1986864 SHAf536f4b92ea91414f478b8160e684070b0b1a3b161ae5e1c4e1c42d8c5cccc5e; ELF808e33ced913736f743ca17db5493af6c680ad48b38d4b4cf8bd93d879ca30e8. Exact472CI API reported no runs; localSDK passed, no CIgreen claim.

## Authority and ownership

Root sole live operator. Agents no serial/controller/RF/service actions. phase2_status investigating whether cancelled/checkable can coexist with newly selected OTA slot before reset; also updates GitHub status. checkpoint_review authored retention; phase2_independent_review approved source and auditing473package.

Sid authorized ongoing engineering phase flashing/publication. Visible ENGINEERING BUILD — INCOMPLETE and exact warning “WARNING: Please do not load these builds, these are still a preview of the final approach.” Normal release eligibility; NO alpha/prerelease/channel/temporary software blocks. HA changes/restarts not authorized. RF allowed ONLY until2026-11-11T04:24:29Z, chat BEFORE and AFTER every session; NONE started. No pairing/calibration/limit tests authorized. Don’t disturb occupants.

Human GitHub exception: verify SidShaytay via env -u GH_CONFIG_DIR /home/linuxbrew/.linuxbrew/bin/gh. Networkgit uses env GIT_CONFIG_COUNT=0 SSH_AUTH_SOCK=/run/user/1000/gcr/ssh /usr/bin/git with explicitSSH URL. No force/retag. SDK cached offline: PLATFORMIO_CORE_DIR=~/.platformio, ESPSOMFY_TOOL_CACHE=~/.cache/espsomfy-lilygo, shared issue-25-design/.pio/libdeps, host-owned builddir and failing127.0.0.1:9 proxies. Canonical tools/build-lilygo.sh; no download/serial targets. Previewcompiler containerrocm-7.2.3. User services require XDG_RUNTIME_DIR and DBUS_SESSION_BUS_ADDRESS. See AGENTS.md/docs/flashing.md before live plan.

Navigation discussion: top quick entries / Browse all (recommended inclusive label) / This controller → Configure, Maintenance, Status(existing wake screen), About, Diagnostics. Not approved to implement. No new unreadable-settings wizard; existing recovery retained. Light is garage lamp; show unavailable. English-only editor. Wiredrelay tilt settings/lift preserved, wiring/tilt mapping not established. Previous decisions/history archived in issue-25.log; do not routinely reload.

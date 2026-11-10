# Issue #25 — software delivered; hardware qualification remains

Goal: complete the agreed LCD, button and rotary functionality. Preserve settings, identities and counters. The approved navigation and 30-minute updater/retries are implemented, reviewed and installed in engineering **4.8.0**. Do not mark the goal complete: physical assessment, raw/normal-reboot verification, the new GitHub flow and combined-load/idle qualification remain. Issues #25/#12/#30 are open and current.

## Immediate dependency

Sid is checking the new menus. The pending async question asks whether the three quick entries, Browse all and This controller sections/Back/wheel response are good. Root promised **NO USB maintenance or active load probes until Sid says finished**. Existing passive logger may continue. Do not infer completion from time elapsed.

User explicitly approved the navigation and resumed work. Other menus retain normal120-second security expiry; confirmed GitHub installs alone can run30minutes without idle cancellation. Temporary downloads retry within the original deadline, respecting rate limits. Credentials/config changes revoke; safe authenticated Cancel remains; invalid certificates/digests/identity and flash writes are not retried. No background auth renewal. No RF session has started.

## Installed and candidate

**Installed480:** immutable source/tag `54fe4b57a1679b12a14077b9b2d648530c3dccba`, app1. Image1,991,808B SHA`a7d9402578cb75dc1323f5a9aeb888a1190092561e099a70d236c3527b985b0e`; ELF`c24034609b2517cdb0b9d3f543a22d671da83ef78f485095fe0dd794b88eb4ed`. Root used ONE manual firmware upload (70193 terminal0/202). Watch64655 terminal0 verified exact app1 boot21:07:45UTC, SDKhealth confirmation and network480. Independent after-state proof confirms all5settings/entities/effective counters/server identity unchanged and radio initialized. RawOTA22VALID is expected, NOT yet read. Never claim this manual install exercised the new480 GitHub flow. Earlier473 was a successful actual LCDGitHub installation.

**Candidate481:** version/cache-only successor to exercise that actual new flow without bypassing the version guard. Source/tag`23b239caee9e2532deee2ebd05f72dfc3d6bdc25`, clean isolated `issue-25-engineering-v481`. Canonical SDK93609 terminal0 PASS49.033s; two assets/package independently approved. Image1,991,808B SHA`08454ecf284b6b7ab06215a8c88ca0ce67aded76f822546a409938fbab546653`; ELF`c5c625aba21575005a1764aa05c608e077b426b1d554452f4e887d5041eeaa0b`; JSON`620af3d27eb902ca88324a3fd0d11ec9390dcbc512f9e42338256646191c4d6e`. Build/v481-tagged-package/artifacts and build/v481-release-assets. Not published or installed. Reuse unchanged480 tests. Main79a3811 is pushed and has corrected static480+ guide paths;481 is not integrated or published yet.

480 gates passed:1291native/1optional skip,118web, release scripts, canonicalSDK50.430s and genericesp32dev39.974s, fullactualencoder/GFX/auth/capture/FIFO,47navigation cases,12form cases,43installer/30controller/2developer-file,69worker+threaded ownership and independent mutation checks. Logs `/tmp/i25-v480-*`, `/tmp/i25-install-*`. No repeat on unchanged code. Always retain ENGINEERING BUILD—INCOMPLETE and exact warning. Normal release eligibility, no alpha channel.

## Recovery and operations

Private root `~/.local/state/espsomfy-lilygo`:
- `phase480-preflight-75ewxueo`: completed full16 raw5ae709ab… proves473app0/OTA21VALID and entire retained471app1, all16NVS/EVERYLittleFS/effective counters. Normal473boot/state audit passed. Its deploy child contains public480/package/prewrite/rawlog/passive/postinstall proofs. Current480 pending guards remain; **NEVER reuse its normal473 helper**.
- `phase481-preflight-xh9ijb6n` (PHASE481_PREFLIGHT_CURRENT): OFFLINE combined post480/next481 recovery plan. Root independently reviewed exact plan`a391b2…` and helper`c90e2d…`; execution_approved=false. Requires Sid-finished evidence, fresh480 before-capture, announced exclusive maintenance and ONEfull16read. Then new independent raw proof must establish actualapp1a7/ELFc240/OTA22VALID, entireapp0confirmed473 vs prior raw, all16NVS/EVERYFS/counters/layout/upper8 before guarded normal480boot. Oldguards untouched. No current new raw/captures or write approval.

After Sid finishes: one combined read/proof/normal480boot and fresh-state comparison; publish reviewed engineering481, prepare exact LCDGitHub install plan/observation, then ask Sid to confirm Install once and walk away while Root monitors. No remote-install bypass or repeated upload. Post481 raw/normalboot and remaining qualification follow.

## Authority and ownership

Root alone operates controller/USB/services. Ongoing phase flashes/publication authorized; no repeated permission needed. No HA changes. RF permission expires November11 04:24:29UTC (November10 8:24p.m.Pacific); announce before/after EVERY RF session. No pairing/calibration/limits tests invented. Current work sends no RF.

All three agents are finished unless assigned: phase2_status authored implementation/docs; checkpoint_review actual Runtime tests; phase2_independent_review package/raw/state/independent source reviews. No agent device authority. Preserve worktrees/tags/private data. Human GitHub exception: verify SidShaytay with `env -u GH_CONFIG_DIR /home/linuxbrew/.linuxbrew/bin/gh`; Git network uses GIT_CONFIG_COUNT=0 and SSH_AUTH_SOCK=/run/user/1000/gcr/ssh. Workflows are disabled_manually; local gates passed, do not claim CI. Follow current AGENTS.md and docs/flashing.md. Detailed history is in issue-25.log; read only when needed.

Current independent offline work is finished. Candidate481 and its package are ready, and Root has conditionally reviewed the next read/reset operators. The only immediate hardware dependency is Sid's physical-check-finished reply; do not probe/reset/load the board while that check is pending. No new permission is needed beyond that missing observation.

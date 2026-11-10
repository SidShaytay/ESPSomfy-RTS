# Issue #25 — active; hardware qualification incomplete

Complete the full web-to-LCD/button/rotary scope, not just Quick access. The normative requirements are GitHub issue25 comment6753845395; all 83 inventory rows are classified in `main/docs/lcd-capability-audit.md`. R1–R7 software is implemented/reviewed; R8 actual GitHub and combined-load qualification remains open. Root alone writes this record. History is in `issue-25.log`; do not load it routinely.

## Current controller — independently verified 4.6.1

Source6697b48cc5e36fb7e07583f1a12af58db79cf008, app0, actual OTA19 VALID. Firmware SHA256 `6c8a15a70dbce731e0b37906b3cd8c6c71cdd9ca9bce587f8560ebf828145c57`; ELF `19a60f3cdd997da6d38ac58c12126e65ffa0e1e172d51563d5ebbead0329920e`. App1 retains healthy4.6.0/source21685bd/image0ff695cc80bdbc7342178e111b1937ea5ffd236b36326fe72a3b2e0ca714435d. Sid finished physical assessment: **response is good**; quiet-assessment restriction ended.

Private base `~/.local/state/espsomfy-lilygo`; `LATEST_VERIFIED_DEPLOYMENT` and `ISSUE25_FINAL_DEPLOYMENT` point to `phase461-preflight-4sxy2gnw`. Final proof `postwrite/final-deployment-verification.json` approved independently: exact image/SDK confirmation, all allocated8MiB readback, retained wholeapp1, all16NVS includingLCDUI, everyLittleFS path/content, effective primary/linked counters, guarded normal reboot, fresh five settings/entities/counters and radioInit true. Upper unused8MiB was NOT postread; fresh full16MiB prewrite backup remains preserved. Last USB read1963 and bootback36891 are TERMINAL0. Only passive read-only diagnostics logger remains; no operator descriptor. No RF/motor/shutterGPIO/HA actions.

## Published Quick access software

Immutable public v4.7.0 tag/source **b31f727172e3de0141de347b28af58806150c7d6**. Main nowa3fbbbae0e584fcec9a9b6b563e3dc3430a23d6f adds reviewed docs and CI-test portability only; pushed successfully. Preserve Sid's public preview warning on the release. 4.7.0 is NOT installed.

Exact approved package: `issue-25-quick-access-integrated/build/v470-tagged-package/artifacts`; two published OTA files in `build/v470-release-assets`. Firmware1985680B SHA `424ce7fc66d0d7f40f98f71b570f3ecc41cbbad268e057ea1663b186414fc0f5`; ELF `13b5deb8d3e2b641c8a5ba690338bb41902f2c38467b4d66797d8357120d2187`. JSON708B SHA `f0691e6fc98240b3df88113ada97ab974baff85110b7a96257dcd4ca228e17c3`. Public downloads verified after release CI finished. Old `build/v470-package` is dev-identity compile evidence ONLY, never flash it.

Checks: 1281native passes/one optional backup skip,118JS, actual encoder/GFX/auth/capture/FIFO, CRC/NVS-authority/SPI-budget mutations, tagged canonical SDK49.668s, genericesp32dev40.803s, version/release/manifest checks and independent package review. New9commits gitleaks clean. Do not repeat unchanged checks. Release CI42869801139 failed because rg absent; three test assertions now portable grep, independently RED→GREEN without rg. Do not claim old tag CI green or retag it.

Quick Home shows group1/device1/device2 from current metadata: Both rooms/Master Bedroom/Master Bathroom. Opens canonical controls/STOP without command, Back direct Home; Browse retains full deep navigation. Protected pin/remove/reorder uses explicit Save, one separate LCDUI key with commit/readback, no boot/navigation writes. Missing seeds RAM only; empty/corrupt/unreadable do not. Source/actual tests and docs reviewed; no alias/UUID/tombstone expansion.

## Next executable stage — fresh full recovery, then LCD GitHub install

`PHASE470_PREFLIGHT_CURRENT` points to **phase470-preflight-w7pwc5id**. Exact candidate, readonly capture/proto helpers and port file copied; **NO read/write/download/install started**. phase2_independent_review prepared `read-reset-plan.json`, `normal-bootback.py`, `operator-files.sha256` OFFLINE. phase2_status is separately reviewing them before live execution; inspect its actual report. Root is sole live operator.

Next: inspect peer approval and bound hashes → announce temporary display/wheel outage → fresh read-only before capture/portable backup → pause logger/exclusive port → ONE pinnedesptool4.12 full16MiB read → independent raw/layout/allNVS/allFS/counter verification → guarded normal4.6.1 bootback → fresh state compare. Bootback needs NEW independent-backup-verification.json bound to full16MiB SHA/currentapp0VALID19/app1old4.6.0; rejects pending/write markers. DTRfalse before open, clear HUPCL, hold same descriptor until exact ELF+network; errors retain descriptor even if logging fails. Never reuse old helper approvals/one-attempt markers. No firmware write until a separate exact GitHub4.7 write/recovery plan is reviewed.

Actual GitHub Check/Install is LCD-ONLY; no remote HTTP adapter exists. After recovery is ready, Sid must operate the dial for real check/install qualification. Do not invent a debug bypass or substitute another manual upload for R8. Observe actual TLS/full staging with bounded capture/reconnect, local editing/cancel, recovered memory and two-hour idle as specified. No RF/motor commands to prove controls; unsafe physical tests remain forbidden.

## Authority and tools

Sid explicitly authorized phase flashes and publication of reviewed4.7.0. No extra permission per already-authorized flash. No RF/motor/shutterGPIO/scans/programming/calibration/HA changes or occupant disturbance. Keep credentials, identities/counters and raw captures private. Announce ROM maintenance before it; screen/dial stop temporarily.

Human GitHub exception: verify SidShaytay with `env -u GH_CONFIG_DIR /home/linuxbrew/.linuxbrew/bin/gh`; machine account suspended. Git network requires `GIT_CONFIG_COUNT=0 SSH_AUTH_SOCK=/run/user/1000/gcr/ssh /usr/bin/git` and explicitSSH URL. No force/overwrite/retag. Host user service needs XDG_RUNTIME_DIR=/run/user/1000 and DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus.

HOST SDK cached only: PLATFORMIO_CORE_DIR=~/.platformio, ESPSOMFY_TOOL_CACHE=~/.cache/espsomfy-lilygo, sharedlibdeps=issue-25-design/.pio/libdeps, explicit host-owned builddir, failing127.0.0.1:9 HTTP(S)/ALL proxies. Preview compiler is container rocm-7.2.3; its outputs are container-owned. Native wrapper ~/.cache/espsomfy-lilygo/native-toolbin. Preserve all worktrees/data.

## Latest user questions

Answer entire navigation top→leaf with Quick Home, explain faster redraw, and assess upstream compatibility. checkpoint_review supplied complete source tree in messages. Redraw: old one4-row band per loop,43passes at31Hz≈1.4s even clean; new skip-clean + up to8dirty bands under8ms admission/10ms clean-validation limit, release SPI after eachband, preserve completed-frame/auth/STOP/capture checks. No SPI-clock increase.

Current cjkas latest releasev4.0.2/mainf17eb4c7; main only3commits ahead (README/version assets). Our exact07bff ancestry/protocol2/format24 remain; Quick4.7 changes no audited protocol/config contracts. Canonical bundling/UI isolated; generic filesystem retained. Shared persistence/counter/RF/queue/updater changes substantial. Upstream ADR0007 removed GitHub updater; ours pins Sid repository, so merge needs policy agreement and upstream-generated assets/versions. Recommend minimalboard→small sharedfixes→optionalLCD→optionalbundle/updater; no new upstream outreach/PRs authorized. No safe downgrade/drop-in binary claim. GitHub25/30/12 updated; all remain open.

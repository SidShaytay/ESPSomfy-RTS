# Issue 23: native group percentage moves

[Issue 23](https://github.com/SidShaytay/ESPSomfy-RTS/issues/23) spans firmware and the HA integration. Completed-feature policy: `main` defines completion and is the baseline for subsequent features. Sid accepted this deployment's combination with the unfinished LCD work as a one-time exception; this does not declare LCD work complete.

## Authorization and limits

Sid authorized fresh backup and firmware flashing on November 6, 2026, then explicitly approved the matching HA installation/restart. No assistant RF/motor commands, pairing, calibration, motor-limit changes or identity changes were authorized or performed. Sid manually tested after both systems were ready, reported failures, and explicitly instructed: no more assistant flashing. The service window is closed; perform no further flashing, reboots, RF/motor commands or live testing. Keep raw backup/configuration, identifiers and credentials private.

## Deployment

- [x] G23-1: native group planning, actual TX timing/completion, per-member Stops, safe fallback/replacement, refusal preservation and stale-work cancellation.
- [x] G23-2: schema-advertised `groupTarget` operation 55, strict input validation, bounded per-connection replay protection and backward compatibility.
- [x] G23-3: HA native percentages with old-firmware fallback; target commit `34eb042` on the HA fork's `codex/issue-23-group-target` branch.
- [x] G23-4: integrated image source `c41cfc4` on `codex/issue-23-deployment`, combining group feature `0d07d06` with LCD/category-fix baseline `11ef3d4`. Canonical environment `lilygo-t-embed-cc1101-plus-ui`; enabled local controls retained. Firmware SHA256 `b4a9344a540e68fba6b4e24dffa50fa23c900dbce2ed59de2d591ea0e3f40ebf`, 1,413,136 bytes.
- [x] G23-5: fresh full 16 MiB backup, portable export, five settings sections, independent NVS/config extraction, partition MD5, both app image integrity and OTA-selection CRC verified. Installed via firmware-only inactive-slot A/B update. NVS, filesystem, bootloader, partition table and prior active firmware were preserved by the update path.
- [x] G23-6: exact HA `34eb042` installed atomically; all 21 file hashes matched, configuration check passed on HA 2026.10.2, and one restart completed. Three consecutive read-only samples found all three covers available and idle; HA running normally with ESPSomfy loaded. Sid was told both systems are ready for unplugging/manual tests. Monitoring stopped; no motor services were sent.
- [!] G23-7: physical validation FAILED. Sid reports that the shutters do not move in sync, do not reliably trigger together, and do not travel the same distance. Sid is out of testing time and forbids further assistant flashing. This report does not establish whether the cause is planning, fallback selection, transmission timing, RF delivery or travel estimates. Do not mark issue 23 complete or claim reliability. Next: offline investigation only; no further device operations.

## Verification and recovery

Integrated build: 699 native tests passed, one optional private-fixture skip; 63 web tests passed. UI renderer/runtime/capture/production-adapter, panel/radio SPI and reserved-GPIO harnesses passed. HA: 142 tests passed. Mutation checks detected policy/eligibility/capability/replay regressions. See [flashing procedure](../docs/flashing.md).

The installed device advertises schema `c109d9d8` and operation 55. Read-only checks at 187 seconds uptime found radio initialized at 433.42 MHz, LCD commands enabled, no crash markers, and all five settings sections, entity configuration, estimates and counters unchanged. No restart occurred during observation.

Private recovery/deployment evidence is selected by `~/.local/state/espsomfy-lilygo/ISSUE23_CURRENT_INSTALL`. HA preparation and rollback evidence are under `~/.local/state/espsomfy-ha-homekit/deploy-issue23-20261107T030226Z`. Fresh recovery precedes future writes; backups become historical once real commands advance counters. Never restore stale counters unchanged.

## Offline repair checkpoint

- [x] G23-8: reproduced production-path failures in `issue-23-debug`, branch `codex/issue-23-debug`, based on completed `main` (`953927b`, documentation `e17ecc1`) plus the existing unfinished issue-23 feature. Saved configuration selects a real native shared start; ordinary Down and target-100 encode the same command. No idle-start adapter fault or RF reception cause was established.
- [x] G23-9: reviewed offline fixes committed and validated. `a3639fb` extracts five controller TX methods unchanged for real host integration coverage. `0a0cb2d` drops known-untransmitted jobs instead of emitting misleading started/aborted events and inventing travel. `40dcd8d` coalesces the latest group target during an unfinished owned start; same-direction active targets retain the shared movement/clock without extra RF; reversing targets retain Stop/completion guards. Elapsed travel is refreshed before target-direction decisions, including safe restart from a predicted self-stopping limit.

New production regressions reproduced rejected final slider targets and two unnecessary individual Stops. Two delayed-loop regressions failed with stale20% estimates instead of current30%/limit100%, then passed. The production harness executes actual group/shade/remote/controller TX code with host-only hardware/persistence/startup/output boundaries. Airtime tests include actual packet-duration formulas and the100ms inter-job gap; close Stop deadlines still serialize and may overshoot. These are model limits, not evidence of received RF.

Final validation: **728 native tests passed, one optional private-fixture skip;63web tests passed**. Enabled UI, headless Plus and generic ESP32 builds passed. Independent review found no required changes. Removing canonical shared-ticket preservation broke seven production tests; latest-target and unavailable-transmitter mutations also failed as expected, with sources restored.

No hardware/USB/serial/controller/HA access, flashing, reboots or RF/motor commands occurred during this repair. Installed firmware and HA were not changed by this work. The exact cause of every user-observed failure remains unproven; keep issue23 open and the unfinished group feature outside main. Next: retain offline changes and continuity in issue23; physical confirmation awaits a later user instruction, never an automatic deployment.

A separate display task documented a later installed combined source `b1ad2e4`, preserving groupTarget55, after the failed test. Its source/artifact record is selected by `LATEST_UI_DETAILS_POWER_INSTALL`. This does not explain the earlier physical failure and does not authorize live inspection or further writes.

## Independent review fixes

- [x] G23-10: independent offline review of `9444072` (issue #23 comment 6712490533) confirmed My could reach idle motors and that commands were refused during pending starts. Fixed on `codex/issue-23-review-fixes`: a press still queued is cancelled and replaced, never stopped with My (group target, member Stop and group Stop); Stop during an unfinished start cancels a queued press or stops on-air members after it finishes; group Up/Down replace an unfinished start; member targets/Up/Down and group targets coalesce during any unfinished start; pending starts time out after 30 s. Same-direction targets need no RF for any active movement; when every mover must stop, members restart with one native press; intermediate targets within 1% count as reached.

The saved configuration confirms the test travel times (Bedroom 23.75 s up / 22.10 s down, Bathroom 21.15 s up / 19.30 s down); the issue text has up/down swapped. Twelve new production regressions failed against `9444072` and pass now. Validation: 745 native tests passed, one optional private-fixture skip; 63 web tests; UI Plus, headless Plus and esp32dev builds. HA unchanged; its 142 tests still pass against `34eb042`. Not flashed or physically tested.

Next ID: G23-11.

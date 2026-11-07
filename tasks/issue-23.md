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

Next ID: G23-8.

## User-observed outcome — November 6, 2026

The software/build checks and read-only installation checks passed, but the subsequent physical behavior did not meet the feature requirements. Preserve the installed firmware/HA revisions and recovery evidence. No rollback, HA restart, firmware change or additional motor test was performed after this report.

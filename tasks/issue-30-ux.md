# Issue 30 — UX delivery and deployment

Objective: implement Sid’s November 10 LCD feedback, include current main and web dark mode, preserve controller state during deployment, and close #12/#25/#30 while carrying unfinished qualification into separate issues.

Acceptance is complete: reviewed code is on main; engineering 4.9.1 is published and installed; exact first-boot health, full raw recovery, normal reboot and fresh state comparisons passed; GitHub records are updated. Remaining physical/load acceptance is #33 and the historical 4.7.3 panic investigation is #34. No delivery blocker remains.

## Authority and boundaries

Sid authorized implementation, GitHub updates, necessary flashing, subsequent self-operated flashing, and closing #25/#30 with unfinished work extracted. Use 4,000,000 baud for future esptool transfers. Root alone operates the board, USB and services. No assistant motor/RF/shutter-GPIO, programming/calibration or HA actions were authorized or performed.

Use plain `gh` after confirming active SidShaytay authentication. Git network commands use `env SSH_AUTH_SOCK=/run/user/1000/gcr/ssh git`. Public writing for people other than Sid uses `🤖 assisted`. Preserve all existing worktrees, branches, tags, state and counters. The older issue-25.md is historical.

## Delivered behavior

- Brightness includes 1, 2, 3, 5 and 8%, then 10–100% in 5% steps; unchanged saved byte levels remain valid.
- Diagnostics use RAM (free), RAM (lowest free), Largest free block, Ext RAM (free), and FW loop. Free/total describe the same pool; loop rate and maximum duration describe the firmware loop.
- Home and Quick access use four full-width icon rows. Names are primary; duplicate, indistinguishable fallback or pixel-clipped names retain compact IDs. Empty shortcuts have their own muted edit affordance.
- Rooms open directly; Back restores the correct origin. Controls/action labels are consistent. Rotary editors provide visible letters, Shift, numbers, symbols, Space, Del and Done; Back preserves drafts and dirty forms offer Keep/Discard. Secret masking, wiping and authorization guards remain enforced.
- Wi-Fi rows prioritize SSID, signal and security, retain full SSIDs, and show BSSID in review. Capture CLI failures explain protected, busy and unavailable outcomes. Protected captures remain protected.
- Main’s merged web System/Light/Dark theme is included, with persisted browser preference and system-theme support.

## Integration and verification

Main includes brightness f4a7a13, diagnostics e7a094a, navigation/editor/Wi-Fi 752559f, dark mode 60bcaaa and four-row/name refinements e4e9d8d. Clean release source/tag v4.9.1 is b4d06cf9d72b828837b4ad99026dbc6930b1d768. The v4.8.1 tag and its preserved worktree remain untouched.

Validation: 1,318 native tests passed with one existing optional skip; 122 web tests passed. Actual encoder/GFX/authentication/capture/FIFO/updater regressions and independent source/package reviews passed. Mutations proved the unseen-unlock guards and wide-name disambiguation checks. The canonical 4.9.1 package build took 50.565 seconds; manifest, source, compiler, bundled assets, bootloader and partition identity checks passed.

Package: main/build/v491-package/artifacts. Firmware SHA256: 6a269e3bf6ec00fdb9f6a4a7c50e2a8e788ab9f42b52a0396bc587e95a583ba0. ELF SHA256: e8a142b6459dceeee320c3acc2395898ba17a9fad64d3dd0d5d0bea9aca34f06. Both release assets are published and their public digests match.

## Hardware evidence

1. Sid installed 4.9.0 through the physical LCD GitHub updater from 4.8.0. Exact app0 ELF, SDK health confirmation before a later boot and network version passed. Settings, entities, counters and identity matched. Full 16 MiB readback proved VALID OTA23/app0 and the entire retained 4.8.0 app1; a normal reboot and fresh state comparison passed. Issue #12 is closed.
2. Root approved the exact 4.9.1 plan (30dc832f…) and operator inventory (9feb0234…) after independent review. The observer started before the single firmware-only upload. Four permanent guards disabled earlier boot helpers before POST. HTTP 202 was treated as staging acknowledgement; the exact manual operation reached written/restart, then the first app1 ELF, SDK confirmation before another boot and network 4.9.1 were observed.
3. Fresh before/after comparisons passed across all five settings sections, devices/groups, rooms, effective counters and server identity. Post-install read plan52f4a413… was bound to actual evidence and root-reviewed. Hardware/security matched the known Plus/S3/16MiB/8MiB-PSRAM board. One full 16MiB read at 4,000,000 baud took174.73 seconds.
4. Independent raw review proved exact 4.9.1 app1/VALID OTA24, entire retained 4.9.0 app0/VALID OTA23, all 16 NVS namespaces and active CRCs, all9 LittleFS files, effective counters and fresh model state. Bootloader/table, coredump and upper8MiB are byte-identical. NVS/LittleFS physical storage bytes changed, but every decoded value and file matched; the initial stricter raw-byte comparison was reviewed and required no state exception.
5. One normal 4.9.1 reboot returned the exact ELF and network version. The final stable capture matched all settings/entities/rooms/counters/identity. Passive logger restored; no pending image. Proof: normal491-state-verification.json in the private post491 recovery directory.

Current qualified recovery: ~/.local/state/espsomfy-lilygo/phase491-manual-f63qynb4/post491. Earlier 4.8.0/4.9.0 boot helpers remain permanently guarded. Any later candidate must guard the current post491 stage before another write. The preserved CRC-valid 4.7.3 arduino_events coredump was matched to its historic ELF/log and is tracked in #34. No new panic was observed in these verified 4.9.x boots.

## Completion

- [x] #12, #25 and #30 closed with deployment evidence and historical records retained.
- [x] #33 carries remaining physical navigation/editor assessment and combined TLS/staging/screenshots/reconnect/edit/fragmentation/long-idle measurements.
- [x] #34 carries the historical panic cause investigation.
- [x] Main and the working branch include all completed feature code and dark mode. Final documentation and this record are included in the completion commit.

Backups remain private under ~/.local/state/espsomfy-lilygo. Older LATEST_* pointers refer to an early OneDrive copy; do not assume they identify the current recovery set. Do not move, delete or upload recovery data without Sid’s request. Keep progress updates concise and regular. Delivery is complete after the documentation commit is pushed to main; report the installed version, closed issues and follow-up links.

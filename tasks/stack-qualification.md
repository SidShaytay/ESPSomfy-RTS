# Stack qualification — authorized work complete

Objective: audit real-board stack pressure across RF, TLS/download-only staging, LCD editing, Wi-Fi reconnects, combined activity and longer recovery; publish sanitized evidence in #33 after the bounded #35 baseline. The tested source is 304774c, version 4.1.4, on T-Embed CC1101 Plus.

Sid authorized USB automation, necessary backups/install/reboots, deliberate reconnects and bounded UP-only RF while shutters were already open. No pairing/calibration, motor-limit, relay or HA changes were included. Sid subsequently requested proposals and documentation only; do not implement those follow-ups without a new request.

- [x] Q1 Captured the initial live state and published the executable plan in #33.
- [x] Q2 Implemented and reviewed opt-in controls/probes and host ACK fixes before testing. Clean ordinary/controlled builds, 1,320 native tests (one existing optional skip), 122 JavaScript tests and the integrated UI harness passed. Historical checkpoints remain in the [checkpoint history](stack-qualification-history.md) and Git history.
- [x] Q3 Installed the integrated controlled 4.1.4 after fresh full backups, exact candidate review and preserved-state verification. First-boot ELF and SDK confirmation passed.
- [x] Q4 Passed isolated UI, three reconnects, repeated complete TLS/download verification, three guarded UP requests, ten-minute combined load and five-minute plus two-hour recovery. Recovery had 260 observations over 7,501.779 seconds; maximum gap 33.872 seconds. State matched with only the three accounted RF advances. User reported about one minute of menu navigation; this was not uninterrupted idle. Fifty post-report observations span 24.5 minutes, with input absence based on the user report.
- [x] Q5 Published the report and four reviewed JSON files; initial remote blob hashes matched. Final evidence includes 3,678 stack records across twelve tasks, with two incomplete records rejected. Three failed harness attempts remain disclosed separately.
- [x] Q6 Returned to ordinary 4.1.4 after a new verified 16 MiB backup. Raw verification proved controlled app1/VALID30 and retained app0/VALID29. One firmware-only app0 upload passed exact ELF, whole-history single boot, SDK health and fresh settings/entities/rooms/counter comparison. Controlled app1 remains retained. OTA31 is expected, not claimed from a post-install raw read. Ordinary ELF starts 24f64ca8; audit controls/probes are absent. Passive diagnostics remain running.
- [x] Q7 Captured the physical LCD Wi-Fi scan separately: networks appeared and Back worked; 80 successful status reads, eight discovery replies, 31 stack records and zero sampled network failures. The 34.548-second user window is not scan latency; capture continued 41.199 seconds afterward. State/counters and all stack minima matched.

Application minima: UDP 684, radio 2,000, updater 3,376 bytes unused. Allocations remain unchanged. Recovery ended with 123,292 free bytes/largest 73,716 versus baseline 123,836/77,812; the bounded 544/4,096 offset remains unattributed, with no continuing decline observed. Wi-Fi-driver, IPC, interrupt stacks, exhaustive failures, other-board hardware and full GitHub installation under combined load remain unqualified.

The report proposes deferred UDP audit formatting, numeric release-build network diagnostics, heap-offset attribution and separate SDK/crash evidence. None is implemented. #33 remains open for those and physical acceptance gaps.

Private evidence pointers: ~/.local/state/espsomfy-lilygo/CURRENT_STACK_QUALIFICATION and STACK_QUALIFICATION_ORDINARY_CLEANUP. Raw backups/logs/frames remain private. Never replay old one-shot operators or restore stale counters. Ordinary cleanup and all live tests are complete; no process or user input is pending.

Final findings and proposal-only plan are committed with this record. Main is authoritative after integration; no further live work or implementation is authorized. Future implementation requires a new request. Next ID: Q8.

Editorial follow-up completed: shortened reports and issue summaries, preserved measurements and a separate technical reference, and withdrew speculative work from the active backlog. Reviewed the conclusions and local links. No firmware or device changes.

# Issue 34 — complete

Objective: identify the preserved engineering 4.7.3 arduino_events stack-canary path and determine whether current builds need a fix. Older builds are evidence only; no backport is requested. Sid's earlier v4.0.x Home Assistant outages are an unconfirmed correlation.

Acceptance: decode with exact ELF/toolchain; trace cause into current main; add a focused regression/resource fix if still present; record evidence and distinguish historical panic from current health and #33 load qualification.

Scope: offline artifacts/source/tests only. No device, USB, HA, resets, flashing, destructive reproduction or fault injection authorized. Keep raw dumps/logs/configuration private. Use SidShaytay under the user-provided temporary account exception.

Workspace: issue-34-panic, branch codex/issue-34-panic, based on main 925311a. Authoritative status: GitHub #34.

- [x] P34.1 Exact archived ELF recovered and SHA verified; dump length/CRC verified; esp-coredump1.18.1 + retained XtensaGDB decoded GOT_IP -> setConnected -> SSDP.begin -> notify -> UDP/WiFi transmit. SP12B above4KiB stack base; notify frame1616B. See docs/historical-473-panic.md. Private evidence: ~/.local/state/espsomfy-lilygo/issue34-investigation/.
- [x] P34.2 Discovery moved to loop through atomic pending flag. Production-method regression failed on original event context and passes WiFi/Ethernet, including real connected()/connecting() and link loss before its callback. Connection-guard mutation rejected. LILYGO + generic ESP32 SDK builds passed; native1318 passed/1 existing optional skip; WiFi-scan and settings-startup harnesses passed. No unrelated test suites rerun.
  Independent review approved with no blocking findings; its optional real-link-state test improvement is included.
- [x] P34.3 Reviewed fix b40473c is signed, integrated into main and pushed. GitHub #34 updated with cause, corrected truncated-hash attribution, validation and limits, then closed as completed.
  No physical deployment authorized or performed. Future builds include the fix; installed firmware remains unchanged. Physical/load qualification stays in #33. No additional work remains in this source investigation.

Next ID: P34.4.

# Issue 35 — firmware stack audit

Objective: audit task stack ownership and usage across current firmware; retain reproducible, sanitized measurements suitable for public publication. Continue the #34 discovery fix; broader physical/load qualification remains #33.

Acceptance: inventory configured task stacks and callback contexts; rank compiler/binary stack frames and inspect risky caller paths; evaluate remaining SSDP callback sends; prepare repeatable runtime stack-margin measurement and record what is measured versus unverified. Fix demonstrated acceptance-blocking defects with focused regressions. Public evidence must omit identifiers, credentials, raw dumps and private configuration.

Scope/authority: source/build/host tests on Vega; user permits using the actual board for the audit, interpreted as read-only inspection/measurement. No flashing/reboot, Wi-Fi disruption, HA changes, RF/motor/pairing/calibration or fault injection authorized. Prepare concrete instrumentation before requesting any required deployment permission. Do not emulate a configured radio identity. Record sanitized usage for publication; do not publish raw data.

Authoritative status: https://github.com/SidShaytay/ESPSomfy-RTS/issues/35.

Workspace: stack-audit / codex/stack-audit from origin/main ef5b1ad (v4.1.0 release). Earlier issue-34 workspace remains intact.

- [x] S1 Vega ESP32 build passed:273 .su files/2074 repository functions; binary prologues confirm large frames. Public baseline dataset binds ef5b1ad and exactELF. Remaining SSDP direct unicast sends and unsynchronized queue/USN ownership are findings, not reproduced crashes. See docs/audits/firmware-stacks.md.
- [x] S2 Opt-in self-sampling hooks for6 roles built; probe frame32B; rate/minimum/allowlist host regressions pass; collector rejects private fields and observed reboot/image boundaries. Native1318pass+1optionalskip; discovery/WiFi-scan regressions pass; default and instrumented firmware builds pass. Source/tooling review approved after fixing stale .su contamination and cross-boot provenance. Fresh run dirs, explicit input scope/config hash and declared-vs-captured ELF metadata now tested.
- [-] S3 Integrate verified source/tooling and compiler report, then produce exact clean-source instrumented candidate. Next: signed commits, main integration/push, clean candidate build and #35 checkpoint.
- [!] S4 Board margins and SSDP remediation remain open in #35. Need: explicit fresh-backup/instrumented-flash/reboot authorization before new live telemetry. No board access or writes performed. Next after approval: verify live identity/layout/features and fresh recovery, install only reviewed firmware into inactive slot on unchanged layout, verify boot/state, then bounded idle/read-only capture. No RF/motor/HA/WiFi disruption.

Next ID: S5.

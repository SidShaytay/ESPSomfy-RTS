# Issue 35 — firmware stack audit

Objective: audit task stack ownership and usage across current firmware; retain reproducible, sanitized measurements suitable for public publication. Continue the #34 discovery fix; broader physical/load qualification remains #33.

Acceptance: inventory configured task stacks and callback contexts; rank compiler/binary stack frames and inspect risky caller paths; evaluate remaining SSDP callback sends; prepare repeatable runtime stack-margin measurement and record what is measured versus unverified. Fix demonstrated acceptance-blocking defects with focused regressions. Public evidence must omit identifiers, credentials, raw dumps and private configuration.

Scope/authority: source/build/host tests on Vega; user permits using the actual board for the audit, interpreted as read-only inspection/measurement. No flashing/reboot, Wi-Fi disruption, HA changes, RF/motor/pairing/calibration or fault injection authorized. Prepare concrete instrumentation before requesting any required deployment permission. Do not emulate a configured radio identity. Record sanitized usage for publication; do not publish raw data.

Authoritative status: https://github.com/SidShaytay/ESPSomfy-RTS/issues/35.

Workspace: stack-audit / codex/stack-audit from origin/main ef5b1ad (v4.1.0 release). Earlier issue-34 workspace remains intact.

- [x] S1 Vega ESP32 build passed:273 .su files/2074 repository functions; binary prologues confirm large frames. Public baseline dataset binds ef5b1ad and exactELF. Remaining SSDP direct unicast sends and unsynchronized queue/USN ownership are findings, not reproduced crashes. See docs/audits/firmware-stacks.md.
- [x] S2 Opt-in self-sampling hooks for6 roles built; probe frame32B; rate/minimum/allowlist host regressions pass; collector rejects private fields and observed reboot/image boundaries. Native1318pass+1optionalskip; discovery/WiFi-scan regressions pass; default and instrumented firmware builds pass. Source/tooling review approved after fixing stale .su contamination and cross-boot provenance. Fresh run dirs, explicit input scope/config hash and declared-vs-captured ELF metadata now tested.
- [x] S3 Signed source/tooling ea798fe+611a6dd integrated/pushed to main. Public report/data published with compile-only limits. Clean source611a6dd instrumented build passed; exact2,000,896B firmware SHA256 cb6c6846eb35fc71cfc43cbec32472195c0ef61a4bed5aea3b1f2fcacd3c4b3c, ELF e708d5ad9cd2381cab011b3e87ca7280de5d15daf4a096e74ba21bf6570abbec; embedded ELF match verified. Private candidate/recovery/read-only capture plan: ~/.local/state/espsomfy-lilygo/ISSUE35_STACK_CANDIDATE. Default image has no StackAudit symbols. #35 stays open for findings/runtime evidence.
- [!] S4 Board margins and SSDP remediation remain open in #35. Need: explicit fresh-backup/instrumented-flash/reboot authorization before new live telemetry. No board access or writes performed. Next after approval: verify live identity/layout/features and fresh recovery, install only reviewed firmware into inactive slot on unchanged layout, verify boot/state, then bounded idle/read-only capture. No RF/motor/HA/WiFi disruption.

Next ID: S5.

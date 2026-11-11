# Initial stack audit

This was the first, limited audit in [#35](https://github.com/SidShaytay/ESPSomfy-RTS/issues/35), using 4.1.2 (`a945a59`) on a T-Embed CC1101 Plus. **Use the [4.1.4 report](stack-qualification.md) for the current conclusion.**

The audit found and fixed SSDP work running on small callback stacks. Network callbacks now defer discovery startup to loop. UDP callbacks copy requests into a bounded queue; loop owns reply formatting and sending. The asynchronous radio scheduler was preserved.

The board completed five minutes idle, five minutes of web/discovery load and one minute recovery. All 60 screenshots and six discovery requests succeeded; no unexpected reset or allocation failure was detected. Settings and counters matched afterward.

The smallest observed application reserve was UDP at 728 unused bytes of 4,096. Radio and updater samples were idle-only; this run did not test active transmission or TLS/download pressure. Those workloads were subsequently tested in the 4.1.4 follow-up.

The result supported keeping the existing stack sizes. It did not establish worst-case margins or explain the older Home Assistant offline reports.

[Runtime data](runtime-stack-v4.1.2.json) · [Compiler report](stack-usage-v4.1.2.json) · [Earlier compiler baseline](stack-usage-v4.1.0.json) · [Measurement tools and limits](stack-audit-reference.md) · [Historical panic fix](../historical-473-panic.md)

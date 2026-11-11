# Stack testing: result and recommendation

**Keep the current firmware and stack sizes. These tests found no new stack bug that needs fixing.** The board is back on ordinary 4.1.4 (`304774c`), with settings and rolling codes preserved.

On the T-Embed CC1101 Plus we exercised transmission, HTTPS checks and complete download-only staging, cancelled LCD edits, three Wi-Fi reconnects, a manual LCD Wi-Fi scan, ten minutes of combined activity and two hours of recovery. No reset or allocation failure was detected in the captured evidence. The recovery included about one minute of menu navigation, so it was not uninterrupted idle.

We recorded 3,678 stack samples across twelve tasks. The smallest application margins were:

| Task | Stack size | Minimum unused |
| --- | ---: | ---: |
| UDP | 4,096 B | 684 B |
| Radio | 4,096 B | 2,000 B |
| Updater | 12,288 B | 3,376 B |

UDP's test logging adds stack pressure. Its result warrants caution if we instrument it again; it does not establish a production failure. Radio remains asynchronous. There is no evidence here that warrants moving task stacks to external RAM.

Free heap settled 544 bytes below baseline, with a 4,096-byte smaller largest free block. It stayed stable during recovery. That does not prove a leak, and I would not spend time chasing it unless repeated use causes further loss.

## Is more work worth doing?

No further reliability work is justified by this run alone. Skip SDK rebuilds, deliberate crash campaigns and stack/buffer refactoring for now. If Home Assistant offline episodes recur, small connection-reason and outage counters would be useful for diagnosis. If we repeat the stack audit, move its UDP logging out of the callback first. Neither change is implemented or required now.

This is evidence for the tested workloads, not a guarantee for every failure path. Wi-Fi-driver, IPC and interrupt stack margins, full GitHub installation under combined load and the other board revision were not measured. RF reception and physical shutter movement were not independently observed.

[Technical reference](stack-audit-reference.md) · [Runtime measurements](runtime-stack-v4.1.4.json) · [Workload results](workloads-v4.1.4.json) · [Test-build compiler report](stack-usage-v4.1.4.json) · [Ordinary-build compiler report](stack-usage-v4.1.4-default.json)

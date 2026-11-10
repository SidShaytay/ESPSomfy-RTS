# LCD completion: remaining qualification

Issue #25 remains open. Installed firmware, host tests and a capability inventory are evidence for their specific checks; they do not establish full hardware acceptance. The [normative requirements](https://github.com/SidShaytay/ESPSomfy-RTS/issues/25#issuecomment-6753845395) remain authoritative.

## Verified installed checkpoint

On November 10, 2026, controller **4.5.2**, source `535a355065ee603ed2e28b91833d87745cbaf44d`, is verified in app0 with OTA sequence 17 marked valid. Exact-image/readback, successful health confirmation, normal reboot and unchanged settings/entities/counters were checked. Its stable `v4.5.2` updater identity is eligible for verified GitHub updates; this does not prove a GitHub download or install.

No newer reviewed release has been published at this checkpoint. Building locally, publishing source/tag/assets, installing an image and qualifying its behavior remain separate stages. Do not bypass release identity guards or treat publication as authorized by this plan.

The reviewed candidate adds everyday repeat hold/release; this is not installed behavior of the 4.5.2 checkpoint. It is implemented and reviewed offline: tap sends nothing, an eligible 800 ms hold emits one reviewed finite request, and release cancels future frames without MY/STOP. Controller, completed-frame and actual encoder/GFX regressions cover invalid input, release, terminal feedback and no rearm while held. These checks do not qualify physical input/RF timing or the combined workload below. Separate/Euro wired-relay tilt is an original driver limitation, not a new LCD implementation or waiver gate. The port retains all modes/settings and ordinary supported lift, discloses unsupported tilt, and leaves RF/GP_Remote behavior intact. Designing new relay tilt actuation is outside this port.

## Completed partial measurement

On November 10, 2026, an earlier **4.5.0 / `dcdb588`** 60-second concurrent read-only run completed 12 LCD screenshots and 12 WebSocket schema/five-settings reconnects: 24 successful operations, no errors, and exact settings/entity/counter comparisons throughout.

| Observation | Result |
|---|---:|
| Screenshot request time | 522–729 ms |
| Internal free heap | 128,004 bytes |
| Lifetime minimum heap | 108,320 bytes |
| Largest free block | 81,908 bytes |
| Sampled loop rate | 45 Hz |
| Maximum loop duration in the two status windows | 29,943 / 31,571 µs |
| LCD RGB buffer in PSRAM | 108,800 bytes |
| Maximum LCD paint / row / SPI bus duration | 6,596 / 1,518 / 1,494 µs |
| Graphics fallback / bus busy skips | 0 / 0 |

Display timing maxima are since boot, so this run does not establish which operation caused them. Raw device records remain private, under the deployment operator's `ISSUE25_QUAL_CURRENT` pointer. No configuration, serial-control, RF, motor, shutter-GPIO or HA writes occurred.

This run did **not** qualify TLS, full-file staging, repeated local editing, physical encoder interaction, Wi-Fi-driver reconnects or long idle. WebSocket reconnects exercise a different path from Wi-Fi disconnection.

## Remaining experiment sequence

Durations below bound the proposed experiment; they are not previously agreed performance thresholds. Record exact firmware identity, load and timing windows, and compare recovered current heap under the same idle conditions. A lifetime minimum does not recover. Largest-block/free-heap ratio is only an indicator of fragmentation.

| Stage | Workload and evidence | Important boundary |
|---|---|---|
| Baseline | Five minutes idle, sparse protocol status and existing private USB diagnostic samples; fresh settings/entity/counter comparison. | Observe the running device without opening a control-capable serial connection or changing its state. |
| Reconnect/capture load | Ten minutes of bounded screenshots plus WebSocket reconnect/schema/settings reads, then five minutes recovery. | Preserve legitimate screenshot refusals: 423 for protected frames, 429 for busy capture and 503 for unavailable resources. Never force unlock or disable guards. |
| Physical editing | Sid repeatedly opens, edits and cancels non-actuating name/settings drafts, and exercises unlock, expiry, Back and wake. | Remote edits cannot prove rotary/button/display behavior. Cancel drafts; any saved-setting experiment needs its own exact agreement. |
| Staging allocation | Under a reviewed upload plan, begin an incomplete firmware upload, overlap captures/reconnects, disconnect, and verify cleanup. | The first chunk reserves the full app-capacity PSRAM buffer. This measures allocation pressure, not successful full-image reception. Completing `/updateFirmware` queues installation. |
| TLS and full staging | With an eligible release identity and repository release, overlap captures/reconnects with the GitHub downloader's own staging. Record TLS allocation peaks, internal free/largest minima, PSRAM use, worker stack, loop and SPI timings. | GitHub and manual uploads share exclusive ownership. Independent TLS and manual-upload operations cannot be combined to simulate this path. A complete install can write the inactive slot and restart. |
| Recovery and idle | Verify operation cleanup and exact settings/entities/counters; observe two hours idle with sparse passive samples. | Extend the soak only when evidence warrants it. Any reset or unintended mutation requires investigation before escalating load. |

Installed 4.5.2 provides a numeric-only `LCD update` line in the existing private serial diagnostics every 30 seconds. These metrics are not exposed through HTTP; `/firmwareStatus` reports the current manual operation's state/progress. The fields are: `minInternalB`, `minLargestInternalB`, `maxPsramUsedB`, `workerStackMinB`, `tlsRequestedB`, `tlsPeakRequestedB`, `tlsInternalPeakRequestedB` and `tlsFailures`.

Worker extrema are sampled; zero means unsampled, not a zero-byte requirement. Stack high-water units are bytes. The global allocator counters measure requested bytes including bookkeeping and other mbedTLS users such as HMAC, not just network TLS or physical SDK overhead. The bounded snapshot copy does not measure the whole loop stack. Collect the existing protocol heap/loop and LCD/SPI timing samples alongside this line; sparse samples alone cannot establish every allocation peak. The presence of these counters does not qualify TLS, full staging or the combined workload. No TLS peak measurements are claimed here.

Stop escalation on unexpected reset or mutation, credential/capture leakage, graphics failure, persistent recovered-memory loss or unresponsive servicing. Preserve evidence and inspect exact operation/boot identity before recovery. Record failures as failures, rather than replacing them with a later successful idle sample.

No step in this document authorizes RF/motor commands, shutter GPIO, scans, programming, HA changes, Wi-Fi disruption or rollback fault injection. Keep existing occupant restrictions. Any eventual flash still requires fresh state/counters, a verified full backup and independent review of the exact write plan.

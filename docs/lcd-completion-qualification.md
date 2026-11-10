# LCD completion: remaining qualification

Issue #25 remains open. Installed firmware, host tests and a capability inventory are evidence for their specific checks; they do not establish full hardware acceptance. The [normative requirements](https://github.com/SidShaytay/ESPSomfy-RTS/issues/25#issuecomment-6753845395) remain authoritative.

## Current stage and last fully audited checkpoint

As of November 10, 2026, controller **4.6.1 / `6697b48`** is running in app0: its exact ELF and SDK health confirmation were observed, with unchanged settings/entities/counters and initialized radio. Sid is assessing the bounded redraw fix; active probes and USB maintenance are stopped. Physical response, allocated-flash readback and normal-reboot verification remain pending. OTA sequence 19 is expected, not yet verified by raw readback.

The previous **4.6.0 / `21685bd`** checkpoint passed full-flash readback, independent state/counter comparison and normal reboot, with app1 OTA sequence 18 verified valid. Its healthy image is retained in app1. A successful SDK health confirmation for 4.6.1 does not replace that final deployment audit.

The installed 4.6 line includes everyday repeat hold/release: tap sends nothing, an eligible 800 ms hold emits one reviewed finite request, and release cancels future frames without MY/STOP. Controller, completed-frame and actual encoder/GFX regressions cover invalid input, release, terminal feedback and no rearm while held. These checks do not qualify physical input/RF timing or the combined workload below.

**4.7.0 / `b31f727`** is prepared and reviewed locally, with a tagged canonical package; it is **not installed or published**. It adds three mixed group/device shortcuts, direct controls and Back, full Browse/settings access, and protected explicit pin/remove/reorder Save. [Actual encoder/GFX/adapter/FIFO regressions](../tests/ui-render/runtime-test.cpp), 1,281 passing native cases (one optional skip), 118 web tests and tagged SDK/package validation provide scoped software evidence. See [daily shortcuts](lilygo-local-ui.md#choose-daily-shortcuts). Its physical assessment follows completion of the 4.6.1 deployment audit and a fresh exact recovery/write plan.

The local 4.6/4.7 source, tags and release assets remain unpublished. Building locally, publishing, installing and qualifying behavior are separate stages. Do not bypass release identity guards or treat publication as authorized by this plan. Separate/Euro wired-relay tilt remains an original driver limitation: the port preserves modes/settings and supported controls, discloses unavailable tilt, and adds no new relay driver or waiver gate.

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

The installed 4.6.1 image retains the numeric-only `LCD update` line in the existing private serial diagnostics every 30 seconds. These metrics are not exposed through HTTP; `/firmwareStatus` reports the current manual operation's state/progress. The fields are: `minInternalB`, `minLargestInternalB`, `maxPsramUsedB`, `workerStackMinB`, `tlsRequestedB`, `tlsPeakRequestedB`, `tlsInternalPeakRequestedB` and `tlsFailures`.

Worker extrema are sampled; zero means unsampled, not a zero-byte requirement. Stack high-water units are bytes. The global allocator counters measure requested bytes including bookkeeping and other mbedTLS users such as HMAC, not just network TLS or physical SDK overhead. The bounded snapshot copy does not measure the whole loop stack. Collect the existing protocol heap/loop and LCD/SPI timing samples alongside this line; sparse samples alone cannot establish every allocation peak. The presence of these counters does not qualify TLS, full staging or the combined workload. No TLS peak measurements are claimed here.

Stop escalation on unexpected reset or mutation, credential/capture leakage, graphics failure, persistent recovered-memory loss or unresponsive servicing. Preserve evidence and inspect exact operation/boot identity before recovery. Record failures as failures, rather than replacing them with a later successful idle sample.

No step in this document authorizes RF/motor commands, shutter GPIO, scans, programming, HA changes, Wi-Fi disruption or rollback fault injection. Keep existing occupant restrictions. Any eventual flash still requires fresh state/counters, a verified full backup and independent review of the exact write plan.

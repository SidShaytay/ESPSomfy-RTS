# LCD completion: remaining qualification

Issue #25 remains open. Installed firmware, host tests and a capability inventory are evidence for their specific checks; they do not establish full hardware acceptance. The [normative requirements](https://github.com/SidShaytay/ESPSomfy-RTS/issues/25#issuecomment-6753845395) remain authoritative.

## Current checkpoint

Controller 4.5.0, source `dcdb588`, has exact-image, normal-boot, valid-OTA and unchanged-settings/counter evidence. Its actual updater identity is `dev-dcdb588eea14`; it intentionally refuses GitHub checking before network access. A clean, release-tagged canonical build is required for that path. Do not bypass the identity guard. Building locally, publishing a release and installing it are separate stages.

Remaining software acceptance includes the remaining hold/release input for everyday repeats (Back/auth/context/timeout cancellation after admission is implemented), and working separate/Euro wired-relay tilt unless Sid explicitly accepts an exception. Settings preservation and an unavailable notice alone do not meet the latter requirement. These are separate from the hardware measurements below.

## Completed partial measurement

On November 10, 2026, a 60-second concurrent read-only run completed 12 LCD screenshots and 12 WebSocket schema/five-settings reconnects: 24 successful operations, no errors, and exact settings/entity/counter comparisons throughout.

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

The installed 4.5.0 image does not expose updater allocation/worker counters through HTTP: `/firmwareStatus` reports the current manual operation's state/progress. New builds add a numeric-only `LCD update` line to the existing private serial diagnostics every 30 seconds: `minInternalB`, `minLargestInternalB`, `maxPsramUsedB`, `workerStackMinB`, `tlsRequestedB`, `tlsPeakRequestedB`, `tlsInternalPeakRequestedB` and `tlsFailures`.

Worker extrema are sampled; zero means unsampled, not a zero-byte requirement. Stack high-water units are bytes. The global allocator counters measure requested bytes including bookkeeping and other mbedTLS users such as HMAC, not just network TLS or physical SDK overhead. The bounded snapshot copy does not measure the whole loop stack. Collect the existing protocol heap/loop and LCD/SPI timing samples alongside this line; sparse samples alone cannot establish every allocation peak. This instrumentation does not qualify TLS on a development image.

Stop escalation on unexpected reset or mutation, credential/capture leakage, graphics failure, persistent recovered-memory loss or unresponsive servicing. Preserve evidence and inspect exact operation/boot identity before recovery. Record failures as failures, rather than replacing them with a later successful idle sample.

No step in this document authorizes RF/motor commands, shutter GPIO, scans, programming, HA changes, Wi-Fi disruption or rollback fault injection. Keep existing occupant restrictions. Any eventual flash still requires fresh state/counters, a verified full backup and independent review of the exact write plan.

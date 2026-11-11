# Firmware stack audit

The historical network-event overflow is fixed in #34. This broader audit checks where current firmware spends stack memory and prepares measurements of the remaining margin. Progress and remaining work are tracked in [#35](https://github.com/SidShaytay/ESPSomfy-RTS/issues/35); combined-load and physical acceptance remain [#33](https://github.com/SidShaytay/ESPSomfy-RTS/issues/33).

**Status:** completed for the bounded audit in #35. The reviewed SSDP fixes are on main, and the exact **4.1.2 / `a945a59`** audit image is installed on a T-Embed CC1101 Plus after two verified recovery captures. First-boot ELF/SDK confirmation and final settings/entities/rooms/effective-counter continuity passed. Source tags are public; this work did not publish a binary release. Broader physical/full-load acceptance remains #33.

## Measurements recorded on Vega

Baseline: main `ef5b1ad`, v4.1.0, `lilygo-t-embed-cc1101`; pinned Arduino 3.3.12 / ESP-IDF 5.5 platform 55.03.312; Xtensa GCC 14.2.0, crosstool-NG `esp-14.2.0_20260121`. The [sanitized compiler/binary dataset](stack-usage-v4.1.0.json) binds the exact ELF and source inputs by SHA256.

The baseline build produced 273 compiler stack-usage files and 2,074 unique repository function records. The final instrumented checkpoint has 2,081 records. The dataset exports the largest frames and selected task/callback functions. A frame is the space one function reserves while active, including compiler inlining. It is **not** the total for a task: nested calls, indirect calls and interrupts need additional room. Separate sequential functions do not all add together.

| Function/path | Compiler frame, bytes | Context |
| --- | ---: | --- |
| GitHub download | 2,784 | Updater worker |
| SSDP XML description | 2,032 | HTTP/web worker |
| SSDP discovery reply | 1,632 | Loop or UDP callback, depending on request |
| SSDP announcement | 1,616 | Loop after #34 |
| Local command dispatch | 1,616 | Loop |
| SSDP departure announcement | 1,568 | Loop/network shutdown callers |
| Local UI runtime tick | 1,536 | Loop |
| Local catalogue page | 1,536 | Loop |
| Update image verification | 1,488 | Updater worker |
| Transceiver receive loop | 1,328 | Loop, distinct from radio TX worker |
| SSDP request dispatcher | 896 | UDP callback; remains active through a direct reply |

Xtensa function-entry instructions independently agree with the large SSDP frames. Only an `entry` instruction at a function symbol is treated as its prologue: embedded jump-table data can otherwise look like a fictitious large frame. Prebuilt SDK/Wi-Fi libraries have no compiler `.su` records in this build. Their binary prologues can be inspected, but the report makes no complete worst-case call-graph claim.

## Configured task budgets

These are configured allocations from the pinned application/SDK, **not measured free space**. The byte units of `uxTaskGetStackHighWaterMark` were checked against the pinned ESP-IDF task header; upstream FreeRTOS documentation may instead describe words.

| Task role | RTOS name | Configured bytes | Runtime measurement hook |
| --- | --- | ---: | --- |
| Main loop | `loopTask` | 8,192 | End of loop pass, including early returns |
| Network events | `arduino_events` | 4,096 | End of network callback |
| UDP input | `async_udp` | 4,096 | End of SSDP request callback |
| Web/socket | `async_tcp` | 16,384 | HTTP middleware and WebSocket callbacks |
| Radio transmit | `radio_tx` | 4,096 | Between scheduler jobs, before waiting/transmitting |
| GitHub updates | `github-update` | 12,288 | Between worker commands |

The follow-up SDK inventory corrected base configuration values to actual task creation budgets: TCP/IP 4,608 bytes, system events 4,608 and high-resolution timer 8,704. The pinned headers and prebuilt archive instructions include non-nano newlib and TCP/IP core-locking allowances. The original #35 capture has no self-sampling hooks for these tasks; additional probes belong to #33. The radio row does not imply any RF transmission test. Idle observations cannot qualify active radio or updater paths.

## Findings and priorities

1. **Keep the #34 fix.** Discovery startup now runs on the same loop task that already owns ordinary discovery sends. This corrects the demonstrated 4 KiB event-stack failure without reserving another large stack or changing the asynchronous radio scheduler.
2. **SSDP reply sending now belongs to the loop.** The UDP callback parses a packet and copies a compact request into a five-entry bounded handoff under a short critical section. The loop performs device lookup, USN formatting and all reply transmission. This removes the overlapping UDP-dispatch/reply/network-send stack chain identified in the baseline.
3. **SSDP shared state now has explicit ownership.** Only the loop accesses the delayed response queue and mutable device USN buffer. Incoming handoff slots contain values, not device pointers or borrowed packet data. Generations reject producers from an earlier end/restart; disconnect clears work. Full queues drop new packets, duplicates coalesce, expired entries are reclaimed, and delayed replies retain the original request deadline across queue residence and clock wrap. Production-method regressions cover these paths and concurrent handoff.
4. **Measure loop/UI and updater margins before resizing.** UI functions have several 1–1.6 KiB frames; the updater has a 2.8 KiB download frame plus TLS/network calls. Their allocations alone do not establish adequate headroom. The updater already has its own stack/heap counters, which should be compared with this audit capture.
5. **Reduce buffers only with explicit ownership.** Several SSDP functions keep 1,460-byte packet arrays on the stack through a send. Owned reusable storage could reduce pressure, but a shared global buffer would create concurrent-write problems. Blindly moving every buffer to the heap would introduce allocation/failure costs.

No blanket stack increases or packet-format changes are included in the audit instrumentation.

## Observed runtime margins

The [sanitized runtime dataset](runtime-stack-v4.1.2.json) contains 98 records from all six application roles, bound to the exact captured boot ELF and compiled image. The [4.1.2 compiler/binary report](stack-usage-v4.1.2.json) identifies the tested instrumented source and effective configuration. No raw logs, identifiers, screenshots, configuration or backups are published.

| Task role | Configured bytes | Minimum recorded unused bytes | Unused fraction | Coverage |
| --- | ---: | ---: | ---: | --- |
| Main loop | 8,192 | 2,932 | 35.8% | Boot, idle, controlled reads/captures and recovery |
| Network events | 4,096 | 1,960 | 47.9% | Naturally occurring callbacks; no deliberate Wi-Fi disconnection |
| UDP input | 4,096 | 728 | 17.8% | SSDP callbacks, including six controlled valid discovery queries |
| Web/socket | 16,384 | 12,652 | 77.2% | HTTP, screenshots and schema-sync reconnects |
| Radio transmit | 4,096 | 3,412 | 83.3% | One idle/startup sample; no RF transmission |
| GitHub updates | 12,288 | 10,856 | 88.3% | One idle/startup sample; no TLS check/download/staging |

UDP has the narrowest observed reserve. The ownership fix removes the demonstrated small-task send chain; these measurements support retaining its existing 4 KiB allocation, not reducing it. They do not prove every parser/network path safe. If a larger guaranteed reserve is required, reduce callback scratch usage under explicit ownership and repeat measurements. This audit makes no blanket stack increase or worst-case guarantee.

The controlled run completed five minutes idle (10 schema syncs at 30-second intervals), five minutes load (60 schema syncs and screenshots at five-second intervals, with six valid unicast discovery reads), and one minute recovery (four schema syncs at 15-second intervals). All 60 measured screenshots returned HTTP 200 with the source-defined 108,866-byte top-down RGB565 BMP. Screenshot latency was 493–2,731 ms (median 607 ms); all six discovery queries replied, at most 54 ms. The existing diagnostics collector and ordinary LAN traffic remained active; the controlled counts do not include their requests.

A host-only assertion initially expected positive BMP height. The endpoint correctly returns height −170. Its completed idle phase and failed harness log were preserved; only the load/recovery phases were resumed after checking the format. Two extra HTTP 200 format checks precede the resumed load. The dataset records that pause instead of presenting the phases as one uninterrupted eleven-minute experiment. Stack records span boot through the final verification reads.

Sampled current internal free heap was 124,224–124,436 bytes during idle/load and 124,424–124,436 during recovery; the final sample returned to 124,436. The largest free block remained 81,908 bytes. Lifetime minimum heap fell from 113,524 to 107,144 bytes under the broader observation/load window; that historical minimum does not recover. Sampled loop rate remained 35 Hz, with a largest reported loop window maximum of 47,403 µs. Sparse status samples do not capture every allocation peak or prove absence of long-term leaks.

The emitted probe's compiler frame is 48 bytes. Maximum observed wall time around a stack scan was 10,285 µs on web, 3,478 µs on UDP, 1,096 µs on the idle updater and 129–144 µs on the other roles. These durations can include preemption; they are not pure CPU time. They exclude role lookup and logging. Loop timing ends before the probe destructor, so it also excludes that final probe/logging cost. Later high-water observations include the logging call chain, but total logging latency was not measured. Ordinary builds omit both probe and record string.

No unexpected reset, canary, allocation-failure or boot-health failure marker appeared after the exact boot identity. Final settings, entities, rooms and effective counters matched the fresh pre-install capture. Both full backups verified the canonical layout, both application checksums/digests, OTA state, active NVS CRCs, LittleFS configuration and current effective counters. The second captured the known-good 4.1.1 image before 4.1.2 replaced the inactive slot; the previously active app0 is retained. No new layout or filesystem image was written.

The first 4.1.1 audit image booted and preserved state but produced no measurements because release log level 2 removed info-level output. It contributes no runtime stack data. The corrected opt-in probe uses serialized `log_printf`, and the build now rejects an instrumented binary without its unique record format. Both the release-level regression and emitted-binary guard reproduced that failure before correction.

Current checks passed: instrumented and normal shared firmware builds, generic ESP32 build of the SSDP fix, 1,318 native tests with one existing optional skip, focused production-method SSDP/discovery tests, release-level probe tests, collector/provenance/privacy tests, fresh-build isolation and version-script tests. Independent source review and deadline mutation checks passed. This covers software ownership and the stated board workload; it does not qualify active radio, full GitHub/TLS staging, physical menu editing, deliberate Wi-Fi reconnect, forced rollback, another board revision or long idle. Those remain in #33.

## Reproduce the compiler audit

Use the repository's existing build prerequisites from [LILYGO.md](../../LILYGO.md), then run from the repository root:

```bash
bash tools/audit-stacks.sh static
```

The script adds `-fstack-usage` to the existing canonical environment and writes `build/stack-audit/static/run-*/usage.json`; `build/stack-audit/static/LATEST` names the last successful run. Each invocation preserves a fresh run directory and updates `build/stack-audit/static/LATEST` only on success, so obsolete `.su` files cannot contaminate a later run. It verifies the runtime record format is present exactly once in an instrumented firmware binary and absent in normal firmware, then records the source-input digest before compiling and refuses the report if those inputs change during the build. The report records the effective configuration hash and audit mode; its input-scope field lists the hashed source/assets/libraries/build tools. It never accesses a controller or serial port. `PLATFORMIO_LIBDEPS_DIR` can point to an existing pinned dependency cache. Generated objects, ELFs and logs remain ignored.

The public baseline dataset hashes only `src/` and the original `platformio.ini`, as its explicit input-scope field states; its effective audit configuration is separately hashed. It was captured before telemetry hooks were added; later reports must retain their own source/ELF identifiers rather than overwrite that historical baseline.

## Prepare runtime measurements

```bash
bash tools/audit-stacks.sh instrumented
```

This adds `ESPSOMFY_STACK_AUDIT=1` and writes an instrumented image/ELF under a fresh `build/stack-audit/instrumented/run-*/objects/lilygo-t-embed-cc1101/`. The `LATEST` file identifies the last successful run. Ordinary firmware builds compile the hooks out. This is a build operation, not an installation instruction; any installation follows [the standard flashing procedure](../flashing.md) with a fresh recovery set and explicit authorization for that stage.

The probe:

- Samples only its current task through `uxTaskGetStackHighWaterMark(nullptr)`. It does not enumerate tasks, borrow possibly expired task handles, allocate heap memory or suspend the scheduler globally.
- Scans at most once per five seconds per active role and logs at most once per 30 seconds per role, including a first sample. Inactive callbacks/workers can remain silent. HTTP and WebSocket hooks share one web-role history.
- Reports the smallest observed unused-stack byte count, sample count, uptime and maximum measured scan time in microseconds. The emitted probe frame is recorded for the exact instrumented candidate; caller frames can also change due to instrumentation. The initial 4.1.1 audit image suppressed info-level output and supplied no runtime samples; 4.1.2 uses unconditional structured output only when opted in.
- Measures scan time through the monotonic timer. `maxScanUs` excludes UART/logging time, which can delay execution and affect the observed stack. The result includes instrumentation effects. Tasks recreated under a role retain that role's minimum until reboot.
- Uses six fixed role labels. It does not print task handles, addresses, names supplied by network peers, configuration, credentials or entity identifiers.

Stack high-water marks inspect untouched fill bytes. They are observations, not a proof of worst-case stack usage; large reserved-but-unwritten regions can make them optimistic. Preserve compiler/binary frame evidence alongside them. Stop and investigate any reset, canary, allocation failure or persistently low margin rather than increasing load.

For a repeat run, use a quiet baseline followed by bounded read-only web/screenshot/WebSocket activity, after reviewing the exact candidate and fresh recovery plan. Record duration, workload counts, covered roles, scan/logging overhead and unchanged state. Do not provoke Wi-Fi disconnection or exercise the identified direct-reply risk as a crash test. Updater staging, active radio, physical edits, installation and recovery tests require their separately authorized stages in #33.

## Export a public dataset

Keep raw serial/network captures in private storage. The exporter accepts an existing log; it does not open the board or start a workload.

```bash
python3 tools/collect_stack_audit.py <private-serial-log> \
  --source <full-instrumented-source-commit> \
  --elf-sha256 <full-instrumented-elf-sha256> \
  --board t-embed-cc1101-plus --workload idle \
  --output <sanitized-output.json>
```

Use the actual board model and exercised workload. Only fixed roles and numeric fields are exported. Private prefixes and unrelated lines are discarded; malformed audit records are counted without copying their contents. Empty evidence is rejected, as are inconsistent per-role histories and observed boot/reset/crash/image-identity boundaries after sampling starts. Continuous capture is required: missing markers can conceal a reboot. Split captures across reboots and retain the exact source/ELF privately. The export lists unobserved roles explicitly. CLI source/ELF hashes are labeled declared metadata. A captured full boot ELF fingerprint is recorded separately and compared with the declared ELF; a mismatch is rejected. Absent that marker, running-image identity is unverified.

Publication requires the sanitized dataset and methodology, not the raw logs, backups, configuration or coredumps. The published runtime dataset above is the sanitized export from this completed capture. Raw evidence and recovery data remain private.

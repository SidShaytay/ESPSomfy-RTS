# Firmware stack audit

The historical network-event overflow is fixed in #34. This broader audit checks where current firmware spends stack memory and prepares measurements of the remaining margin. Progress and remaining work are tracked in [#35](https://github.com/SidShaytay/ESPSomfy-RTS/issues/35); combined-load and physical acceptance remain [#33](https://github.com/SidShaytay/ESPSomfy-RTS/issues/33).

**Status:** ESP32 compiler/binary measurements and host checks are complete for the baseline below. Opt-in runtime instrumentation builds successfully. No new board measurements, installation, resets or simulator runs are claimed.

## Measurements recorded on Vega

Baseline: main `ef5b1ad`, v4.1.0, `lilygo-t-embed-cc1101`; pinned Arduino 3.3.12 / ESP-IDF 5.5 platform 55.03.312; Xtensa GCC 14.2.0, crosstool-NG `esp-14.2.0_20260121`. The [sanitized compiler/binary dataset](stack-usage-v4.1.0.json) binds the exact ELF and source inputs by SHA256.

The build produced 273 compiler stack-usage files and 2,074 unique repository function records. The dataset exports the largest frames and selected task/callback functions. A frame is the space one function reserves while active, including compiler inlining. It is **not** the total for a task: nested calls, indirect calls and interrupts need additional room. Separate sequential functions do not all add together.

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

Other SDK tasks include the TCP/IP worker (4,096), system events (2,048) and high-resolution timer task (8,192). They have no self-sampling hooks here. The radio row does not imply any RF transmission test. Idle observations cannot qualify active radio or updater paths.

## Findings and priorities

1. **Keep the #34 fix.** Discovery startup now runs on the same loop task that already owns ordinary discovery sends. This corrects the demonstrated 4 KiB event-stack failure without reserving another large stack or changing the asynchronous radio scheduler.
2. **Resolve remaining SSDP callback ownership before aggressive discovery load.** Unicast root/UUID/device-type searches still call `_sendResponse` directly from `_processRequest`. The dispatcher’s 896-byte frame and reply’s 1,632-byte frame overlap before the network-library calls. This is a concrete narrow-budget path, not a reproduced second overflow. Multicast searches usually use the delayed reply queue.
3. **Synchronize or transfer ownership of SSDP request state.** The UDP producer and loop consumer access the delayed reply queue without a lock/atomic handoff. Direct replies also mutate a device’s shared USN formatting buffer while the loop can announce that device. The audit identifies unsynchronized shared state; it does not claim an observed corruption. A bounded request handoff to the loop would address both sending context and ownership. Its overflow, duplicate, deadline, disconnect and shutdown behavior need regression tests.
4. **Measure loop/UI and updater margins before resizing.** UI functions have several 1–1.6 KiB frames; the updater has a 2.8 KiB download frame plus TLS/network calls. Their allocations alone do not establish adequate headroom. The updater already has its own stack/heap counters, which should be compared with this audit capture.
5. **Reduce buffers only with explicit ownership.** Several SSDP functions keep 1,460-byte packet arrays on the stack through a send. Owned reusable storage could reduce pressure, but a shared global buffer would create concurrent-write problems. Blindly moving every buffer to the heap would introduce allocation/failure costs.

No blanket stack increases or packet-format changes are included in the audit instrumentation.

## Reproduce the compiler audit

Use the repository's existing build prerequisites from [LILYGO.md](../../LILYGO.md), then run from the repository root:

```bash
bash tools/audit-stacks.sh static
```

The script adds `-fstack-usage` to the existing canonical environment and writes `build/stack-audit/static/usage.json`. Each invocation preserves a fresh run directory and updates `build/stack-audit/static/LATEST` only on success, so obsolete `.su` files cannot contaminate a later run. It records the source-input digest before compiling and refuses the report if those inputs change during the build. The report records the effective configuration hash and audit mode; its input-scope field lists the hashed source/assets/libraries/build tools. It never accesses a controller or serial port. `PLATFORMIO_LIBDEPS_DIR` can point to an existing pinned dependency cache. Generated objects, ELFs and logs remain ignored.

The public baseline dataset hashes only `src/` and the original `platformio.ini`, as its explicit input-scope field states; its effective audit configuration is separately hashed. It was captured before telemetry hooks were added; later reports must retain their own source/ELF identifiers rather than overwrite that historical baseline.

## Prepare runtime measurements

```bash
bash tools/audit-stacks.sh instrumented
```

This adds `ESPSOMFY_STACK_AUDIT=1` and writes an instrumented image/ELF under a fresh `build/stack-audit/instrumented/run-*/objects/lilygo-t-embed-cc1101/`. The `LATEST` file identifies the last successful run. Ordinary firmware builds compile the hooks out. This is a build operation, not an installation instruction; any installation follows [the standard flashing procedure](../flashing.md) with a fresh recovery set and explicit authorization for that stage.

The probe:

- Samples only its current task through `uxTaskGetStackHighWaterMark(nullptr)`. It does not enumerate tasks, borrow possibly expired task handles, allocate heap memory or suspend the scheduler globally.
- Scans at most once per five seconds per active role and logs at most once per 30 seconds per role, including a first sample. Inactive callbacks/workers can remain silent. HTTP and WebSocket hooks share one web-role history.
- Reports the smallest observed unused-stack byte count, sample count, uptime and maximum measured scan time in microseconds. The probe itself has a 32-byte compiler frame in the checked instrumented build; caller frames can also change due to instrumentation.
- Measures scan time through the monotonic timer. `maxScanUs` excludes UART/logging time, which can delay execution and affect the observed stack. The result includes instrumentation effects. Tasks recreated under a role retain that role's minimum until reboot.
- Uses six fixed role labels. It does not print task handles, addresses, names supplied by network peers, configuration, credentials or entity identifiers.

Stack high-water marks inspect untouched fill bytes. They are observations, not a proof of worst-case stack usage; large reserved-but-unwritten regions can make them optimistic. Preserve compiler/binary frame evidence alongside them. Stop and investigate any reset, canary, allocation failure or persistently low margin rather than increasing load.

After the exact candidate is reviewed, the fastest useful board stage is a quiet baseline followed by bounded read-only web/screenshot/WebSocket activity. Record duration, workload counts, covered roles, scan/logging overhead and unchanged state. Do not provoke Wi-Fi disconnection or exercise the identified direct-reply risk as a crash test. Updater staging, active radio, physical edits, installation and recovery tests require their separately authorized stages in #33.

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

Publication requires the sanitized dataset and methodology, not the raw logs, backups, configuration or coredumps. No runtime dataset is published yet because no runtime capture has been made.

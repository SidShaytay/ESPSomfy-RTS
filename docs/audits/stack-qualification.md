# Stack qualification on v4.1.4

[Issue #33](https://github.com/SidShaytay/ESPSomfy-RTS/issues/33) tracks this follow-up to the [completed bounded audit](firmware-stacks.md). Task stack allocations remain unchanged. This follow-up adds active RF, updater TLS/download-only staging, local editing, deliberate Wi-Fi reconnects and longer recovery on the Plus board. The limits below remain part of the result.

## Current evidence

The measured controlled image was **4.1.4 / `304774c`**, including #36. The board has since returned to ordinary 4.1.4 from that same source. Clean ordinary/controlled builds, 1,320 native tests (one existing optional skip), 122 JavaScript tests and the integrated UI harness passed. Exact first-boot ELF/SDK confirmation and fresh configuration/model/counter equality passed after verified full recovery captures. The ten-minute combined run, five-minute immediate recovery and two-hour recovery passed their final checks. Sid reported approximately one minute of rotary/menu navigation without selected actions during the long recovery; this does not qualify two hours of uninterrupted idle. Fifty samples after the report span 1,469.195 seconds (about 24.5 minutes). Input absence during that period is based on Sid’s report, not independent input monitoring.

Completed isolated coverage: five minutes idle with ten samples; three typed keyboard drafts discarded; one display preview cancelled with Back; a private draft expired after 32 seconds; four protected screenshot requests returned 423. Three intentional Wi-Fi reconnects recovered schema/discovery in 6.9–8.1 seconds, with observed outages and unchanged configuration/model/counters. HTTPS release/manifest checks and complete download-only verification passed twice. Updater stack minimum was 3,376 unused bytes of 12,288; sampled TLS requested peak 51,545 bytes, internal requested peak 12,376 and zero allocation failures. Idle-only radio/updater values from #35 are not reused as active coverage.

Initial idle samples had 123,836 free internal bytes and a 77,812-byte largest block. After the first TLS run and matched idle observations, these were 123,296 and 73,716; the next identical TLS run varied by eight free bytes without further largest-block loss. This does not identify the first retained allocation or prove absence of leaks. The long recovery sampled 121,216–123,292 free internal bytes with a constant 73,716-byte largest block, ending at 123,292 free bytes: 544 below baseline, with the largest block 4,096 smaller. No continuing decline was observed in this bounded run; the retained offset remains unattributed. Lifetime minimum heap is a separate historical value and need not recover.

Aborted combined attempts remain recorded. One first tap only woke a dimmed display. Later raw button edges stretched past the 800 ms long-press threshold while awaiting delayed USB output. These are failed harness runs, not completed combined qualification; no RF request was blindly replayed. A host-only status-helper workaround passed nine transport tests and three further board draft cycles, with measured device Down/Up spacing of 158–182 ms. The final combined workload uses a reversible display preview with encoder turns, normal TLS/download staging, read/capture load and a separately guarded semantic UP request. It preserves prior text-edit/RF overlap evidence without claiming a continuous successful run across failures.

## Workloads and published data

The successful combined workload recorded 120 schema/status samples, 120 HTTP 200 screenshots, twelve discovery replies, 299 USB encoder edits of a reversible display preview, verified TLS/download-only staging and one guarded semantic UP request. Screenshot latency was 421–2,798 ms; maximum reported loop-window maximum was 330,124 µs. The lifetime heap low of 90,944 bytes already existed before this successful combined run; its minimum sampled current free heap was 103,668 bytes. All three authorized UP requests across successful and aborted phases are accounted for; final settings/entities/rooms matched with only those counter advances.

Recovery recorded twenty immediate samples and 240 long-recovery samples over 7,501.779 seconds total. The largest observed sampling gap was 33.872 seconds. Final settings/entities/rooms/effective counters matched. Every explicit raw channel was screened for recorded panic, allocation, watchdog, corruption and unexpected-boot markers; none was detected. Twelve exclusive USB sessions were stitched with the passive capture, so handoff gaps remain a limit. Two incomplete audit lines were rejected rather than repaired or counted.

Public evidence excludes raw logs, screenshots, backups, credentials and network/entity identifiers:

- [Runtime stack observations](runtime-stack-v4.1.4.json)
- [Workload and recovery measurements](workloads-v4.1.4.json)
- [Controlled compiler/frame report](stack-usage-v4.1.4.json)
- [Ordinary compiler/frame report](stack-usage-v4.1.4-default.json)

The separate physical LCD Wi-Fi scan passed: Sid observed network results and returned with Back without selecting a network or saving settings. The 80.748-second capture contains eighty successful status reads, eight discovery replies and 31 stack records, with zero observed network-read failures. The maximum successful-status gap was 1.631 seconds. The 34.548-second user interaction window is not a measurement of scan latency; capture continued for 41.199 seconds afterward. Settings/entities/rooms and all three accounted RF advances matched the completed recovery state. No stack minimum decreased; TCP/IP maximum scan wall time rose from 1,256 to 1,272 µs.

Full GitHub flash installation under combined load, secured unlock/saved-setting edits, other board hardware, Wi-Fi-driver/IPC/interrupt stacks and exhaustive error paths remain unqualified.

## Observed stack margins

These are cumulative minimum unused bytes from the current controlled run, including instrumentation. The final export contains 3,678 stack samples across twelve selected tasks, including 31 from the separate manual Wi-Fi scan window. Task allocations have not changed.

| Task | Allocated bytes | Minimum unused bytes |
| --- | ---: | ---: |
| Loop | 8,192 | 2,800 |
| Arduino network events | 4,096 | 1,776 |
| UDP | 4,096 | 684 |
| Web | 16,384 | 10,948 |
| Radio | 4,096 | 2,000 |
| Updater | 12,288 | 3,376 |
| SDK system events | 4,608 | 2,468 |
| TCP/IP | 4,608 | 2,516 |
| ESP timer | 8,704 | 7,572 |
| Idle core 0 | 1,084 | 220 |
| Idle core 1 | 1,084 | 340 |
| FreeRTOS timer daemon | 3,120 | 2,412 |

Keep these allocations for now. UDP has the smallest observed application margin and deserves targeted work before any reduction. Its receive callback reserves 1,056 bytes in both builds, including a 792-byte parsed packet and a 172-byte copied request. The controlled callback reports its watermark before that frame unwinds; nested formatting includes an 800-byte `_svfprintf_r` frame. This is a likely contributor to measurement pressure, not proof of the exact deepest call chain. A useful next comparison is to defer UDP audit output to loop and repeat the same bounded discovery workload. A fill-pattern watermark can miss reserved but unwritten stack space; neither a watermark nor an individual compiler frame proves the deepest call chain fits. Radio stays asynchronous. SDK Wi-Fi-driver, IPC and interrupt stacks remain unmeasured.

Two incomplete `STACK_AUDIT` lines at channel opening were rejected: one in the combined capture and one in USB flush validation. Neither supplied a complete measurement. The cause remains unresolved between capture handoff truncation and producer interleaving. The export retains the rejected count of two.

The pinned build already permits large malloc allocations to use PSRAM, with a 4,096-byte threshold; normal dynamically created task stacks remain internal. Owned download images and larger JSON/TLS allocations can use external memory. This build does not enable PSRAM instruction/constant execution. Keep task stacks internal: moving them would require a separate review of cache-disabled execution and all callers, and these measurements do not justify that change.

## Compiler evidence

Both clean builds use source `304774c96ebc1cc02114d73a54d54c3e841d5479`, Arduino 3.3.12, ESP-IDF v5.5.5 and GCC 14.2.0. Each build produced 275 compiler stack-usage files; the reports extract 2,094 repository functions for the controlled build and 2,080 for the ordinary build. Binary prologue inspection covers 5,977 and 5,966 functions respectively. SDK archives do not supply complete compiler stack-usage records.

The complete download, schema and SSDP response functions retain individual frames of 2,784, 2,032 and 1,632 bytes in both builds. The controlled updater worker frame is 976 versus 704 bytes; UI runtime is 1,552 versus 1,536 bytes. Other compiler frame choices differ in either direction, so instrumentation has no single stack-overhead number. These are individual function frames, not complete task bounds. Probe frames include `observeCurrent` 32 bytes, `printSample` 48, `pollSdk` 80 and `beginSdk` 64; nested formatting and SDK calls are additional.

The controlled firmware is 5,680 bytes larger. Its ELF SHA-256 is `de5dca268001297cd0fba8fbcaf59ab070db1f2e6dbcbdf54a4cc002a6cd0177`; the ordinary ELF is `24f64ca83422f11f6e86b55650ebb906fe6a5582a8714572a651e0a9d1b19907`. Runtime observations belong to the controlled image, whose exact ELF was captured on first boot.

## Return to ordinary firmware

After freezing the test intervals, a new verified 16 MiB backup preserved the current settings and all three RF counter advances. The full read confirmed both application checksums, the unchanged layout, active NVS CRCs and effective counters. One firmware-only upload installed the ordinary image into app0, retaining controlled app1 as the previous slot. No filesystem, bootloader, partition-table or configuration-restore image was sent; normal state persistence and OTA metadata updates remained active. No post-install raw read was performed, so byte-for-byte partition/NVS equality is not claimed. The complete post-write history contained exactly one expected boot, the exact ordinary ELF and successful SDK continuous-health confirmation, with no detected crash/allocation/watchdog marker. Fresh postboot settings/entities/rooms/effective counters matched.

The ordinary image SHA-256 is `92042c42981145c6309cac3688e7cae7268cfb4cc36c374e2dd34509d4ff192a`; its ELF is recorded above. Its binary and observed boot omit the audit record formats and USB command receiver. This is the ordinary build of the tested source, not implementation of the proposals below. Runtime stack watermarks belong to the controlled image; ordinary postboot verification does not create a second full workload qualification.

## Reliability proposals — not implemented

Keep the current task allocations, asynchronous radio scheduling and internal task stacks. The observed margins do not justify reducing stacks, increasing every stack, or moving them to PSRAM. Existing PSRAM-backed owned buffers already relieve large payload pressure. The bounded tests found no reset/allocation failure; they do not explain the historical Home Assistant offline reports or prove all failure paths safe.

1. **Defer UDP audit formatting to loop.** Copy a bounded numeric sample into fixed storage and format it later; keep UART/printf off the receive callback. Its 1,056-byte callback frame remains live during current audit formatting, including an 800-byte formatter frame. Compare paired builds with the same unicast/multicast discovery workload and bounded malformed/error cases, recording callback frames, watermark minima, dropped telemetry and responsiveness. Keep the 4 KiB allocation unless that evidence supports a change. This improves measurement safety and separates audit overhead from normal parser pressure.
2. **Preserve numeric network-event diagnostics in release builds.** Current informational event messages compile out at release log level 2. Recovery produced twelve paired network/system-event samples about 605 seconds apart, matching the enabled 600-second roaming scan path, but event types could not be confirmed. Capture only event/reason codes and monotonic times in the event callback; drain fixed records from loop. Reuse existing timestamps where valid, and add distinct disconnect episodes, successful reconnects, last/maximum downtime, scan source/state/duration and a visible dropped-record count. Existing connection counters do not reliably count outages. Exclude SSID/BSSID/IP/credentials. Test initial connection versus reconnect, duplicate events, failed/busy scans, timer wrap and bounded overflow; then correlate a separately authorized reconnect and LCD scan with preserved state. Change no roaming/reconnect policy before these records identify a cause.
3. **Attribute the recovered-heap offset before a leak-free claim.** Compare warmed-up ownership and internal free-block/largest-block snapshots across repeated identical TLS staging/cleanup cycles. Explain retained release/manifest/SDK state separately from fragmentation; owner counters must return to zero or explicitly identify retained objects. Require stable post-cleanup measurements across repeated cycles before increasing sustained load. Do not treat the 4,096-byte largest-block drop as a 4 KiB leak: total free loss at the recovery endpoint was 544 bytes.
4. **Close the SDK/error-path evidence gaps separately.** A pinned SDK build with compiler stack-usage output and supported, bounded task observation is needed for Wi-Fi-driver/IPC coverage; interrupt stacks need their own method. Current SDK configuration already enables canaries, stack-end watchpoints, watchdogs and CRC-protected ELF flash coredumps with a dedicated dump stack and 64 KiB partition. These protections were not deliberately crash-tested here. Validate recovery/dump decoding only on separately authorized hardware, retain raw dumps privately, and publish only sanitized backtraces bound to the exact ELF. Full GitHub installation under combined load remains a separate qualification case.

These are proposals only. No proposed reliability code change or additional deliberate fault test was implemented in this session.

## Repeatable controls

The USB port normally emits diagnostics only. `bash tools/audit-stacks.sh instrumented` adds measurement; `bash tools/audit-stacks.sh controlled` additionally enables `ESPSOMFY_AUDIT_CONTROL`. Controlled firmware is for an explicitly authorized test session. Ordinary and measurement-only images omit the command receiver and download-only audit API. The build checks the emitted measurement/control markers before accepting a result.

Commands are newline-terminated ASCII: `AUDIT <increasing uint32 sequence> <command>`. The parser bounds lines to63 bytes, drains invalid lines, rejects replayed accepted sequence IDs and does not echo input. One line/64 bytes maximum is processed per loop pass. Unsupported commands cannot install firmware or issue arbitrary RF. Responses contain only numeric fields and fixed role names; raw surrounding device logs remain private.

Commands: `state`, `turn -1`, `turn 1`, `down`, `up`, `back-down`, `back-up`, `release`, `wifi-reconnect`, `update-check`, `update-stage`, `update-cancel`. Button/turn inputs pass through the production UI debounce, visibility/presentation, authorization and service admission paths. Injected held inputs expire after two seconds; `release` restores physical inputs. There is no credential injection or unlock bypass. UI navigation can reach real actions, so scripts must check the expected screen/selection before activation and cancel drafts rather than save.

Reconnect and updater commands require the current authorized, unfrozen local session. Reconnect preserves radio mode and saved credentials and is limited to once per30 seconds. Check uses the production local updater adapter. Stage holds the ordinary exclusive preparation owner and loop admission, refreshes the exact candidate identity, uses the production TLS transport and complete image verifier, then frees locally owned PSRAM. It never enters flash admission, selects a boot slot or restarts. An equal/older compatible published image can be downloaded for this measurement; normal newer-version installation eligibility is unchanged. A prepared status here records verification, not retained installable bytes.

`tools/audit_console.py` is a standard-library USB client. It claims the port exclusively, clears HUPCL, sends no DTR/RTS ioctls, creates a private capture without overwriting an existing file, and makes one command attempt. Missing acknowledgements are ambiguous and must not be replayed. Stop the existing passive collector before opening this client; restore it afterward. Merely connecting sends no command. Separate invocations must continue sequence IDs within a boot. Keep a single connection for a scripted run.

Raw button commands represent held levels, so host acknowledgement latency can change their device-visible duration. `tools/audit_console.py --flush-ack` optionally appends a read-only status query in the same write, consumes another sequence and returns `nextSequence`. This encouraged prompt delivery in the measured run; suspected USB buffering is not proven as the cause, and the helper is not a guaranteed flush. Validate device edge timestamps. It does not replace mechanical-input or physical timing acceptance.

## Additional stack observations

The instrumented loop initializes bounded SDK measurements after normal startup. Idle0, idle1 and the FreeRTOS timer daemon have scheduler-lifetime handles; one foreign scan every five seconds rotates through them. System-events, TCP/IP and ESP timer tasks self-sample through supported callbacks. TCP/IP submission is nonblocking with one outstanding request. SDK callbacks defer numeric records to loop, avoiding UART logging on those tasks. No global task snapshot, scheduler suspension or dynamic Wi-Fi handle lookup is used.

Pinned qio_opi headers and prebuilt task-creation instructions establish these SDK budgets: system events4,608 bytes (2,048 base +512 non-nano newlib +2,048 TCP/IP core-locking allowance), TCP/IP4,608, ESP timer8,704, idle0/idle1 1,084 each (1,024 +60 watchpoint allowance), FreeRTOS timer daemon3,120. IPC0/IPC1 are2,048 each and remain unmeasured. These are task budgets; total allocator/TCB overhead is separate.

Application/SDK self-scans remain limited to one per five seconds per role; output is at most one per30 seconds after the first. Radio/updater audit workers wake while idle to report the persistent watermark after short operations. SDK record timestamps are deferred reporting times, not exact operation timestamps. Startup allocation/queue failures retry at bounded intervals. Wi-Fi driver and IPC tasks remain unmeasured; supported callbacks do not establish their stack margins. These tools currently build the canonical dual-core S3 profile; idle1 handling would need adjustment before a single-core instrumentation target.

## Authorized live sequence and acceptance

Sid authorized the remaining tests and USB automation, including deliberate reconnects and bounded non-moving RF, and confirmed shutters physically open. Recheck current physical assumption, live state and command mapping before OPEN-only transmission. Do not substitute STOP/My at rest: on56-bit RTS, My can move to a favourite position. No pairing, calibration, limit changes, relay testing, HA configuration or stale-counter restore is included.

1. Capture fresh state/counters and verified full recovery; install only the exact reviewed audit firmware into the inactive app on the unchanged layout. Verify first-boot ELF, SDK confirmation and preserved state.
2. Record five minutes baseline. Exercise repeatable non-actuating settings/name drafts, keyboard adjustments, discard, Back, dim and wake through normal UI input. Check final persistent settings unchanged. Software injection cannot qualify encoder mechanics or visual usability.
3. Run three deliberate Wi-Fi disconnect/reconnect cycles with a bounded recovery deadline, recording events and discovery recovery separately from expected socket outages.
4. Run production HTTPS release/manifest checks and complete verified firmware download-only staging. Record phase, allocator failures, TLS/internal/PSRAM peaks, task margins and cleanup. Keep failed admission or transport as a failed result, rather than bypassing a guard.
5. Execute bounded OPEN-only RF through a normally admitted path, with all shutters already open and mapping verified. Record scheduler completion and expected counter advancement; never infer physical RF reception/movement from a queue acknowledgement.
6. Run ten minutes combined screenshot/WebSocket reads and cancelled UI drafts with updater staging and bounded RF, followed by five minutes immediate recovery. Preserve legitimate screenshot423/429/503 responses.
7. Compare configuration/entities/rooms and effective counters, allowing only recorded RF advances. Observe two hours recovery with sparse samples. Stop escalation on unexpected reset, unintended motion/edit, allocation failure, failed cleanup/reconnect or persistent recovered-memory loss.

Public evidence must bind source, effective build configuration and captured boot ELF; distinguish cumulative watermarks from phase observations and static frames from task bounds. Raw backups, credentials, network/entity identifiers and protected frames stay private. Record alternate-board and physical usability limits explicitly. The issue remains open until the applicable live stages have actual evidence.

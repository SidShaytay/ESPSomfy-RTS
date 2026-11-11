# Stack audit technical reference

Supporting details for the [4.1.4 results](stack-qualification.md). These describe the measurement tools; they are not a work backlog.

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

Keep these allocations for now. UDP has the smallest observed application margin. There is no recommendation to reduce its allocation. Its receive callback reserves 1,056 bytes in both builds, including a 792-byte parsed packet and a 172-byte copied request. The controlled callback reports its watermark before that frame unwinds; nested formatting includes an 800-byte `_svfprintf_r` frame. This is a likely contributor to measurement pressure, not proof of the exact deepest call chain. If this instrumentation is used again, deferring UDP audit output to loop would help separate measurement overhead from normal operation. A fill-pattern watermark can miss reserved but unwritten stack space; neither a watermark nor an individual compiler frame proves the deepest call chain fits. Radio stays asynchronous. SDK Wi-Fi-driver, IPC and interrupt stacks remain unmeasured.

Two incomplete `STACK_AUDIT` lines at channel opening were rejected: one in the combined capture and one in USB flush validation. Neither supplied a complete measurement. The cause remains unresolved between capture handoff truncation and producer interleaving. The export retains the rejected count of two.

The pinned build already permits large malloc allocations to use PSRAM, with a 4,096-byte threshold; normal dynamically created task stacks remain internal. Owned download images and larger JSON/TLS allocations can use external memory. This build does not enable PSRAM instruction/constant execution. Keep task stacks internal: moving them would require a separate review of cache-disabled execution and all callers, and these measurements do not justify that change.

## Compiler evidence

Both clean builds use source `304774c96ebc1cc02114d73a54d54c3e841d5479`, Arduino 3.3.12, ESP-IDF v5.5.5 and GCC 14.2.0. Each build produced 275 compiler stack-usage files; the reports extract 2,094 repository functions for the controlled build and 2,080 for the ordinary build. Binary prologue inspection covers 5,977 and 5,966 functions respectively. SDK archives do not supply complete compiler stack-usage records.

The complete download, schema and SSDP response functions retain individual frames of 2,784, 2,032 and 1,632 bytes in both builds. The controlled updater worker frame is 976 versus 704 bytes; UI runtime is 1,552 versus 1,536 bytes. Other compiler frame choices differ in either direction, so instrumentation has no single stack-overhead number. These are individual function frames, not complete task bounds. Probe frames include `observeCurrent` 32 bytes, `printSample` 48, `pollSdk` 80 and `beginSdk` 64; nested formatting and SDK calls are additional.

The controlled firmware is 5,680 bytes larger. Its ELF SHA-256 is `de5dca268001297cd0fba8fbcaf59ab070db1f2e6dbcbdf54a4cc002a6cd0177`; the ordinary ELF is `24f64ca83422f11f6e86b55650ebb906fe6a5582a8714572a651e0a9d1b19907`. Runtime observations belong to the controlled image, whose exact ELF was captured on first boot.

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

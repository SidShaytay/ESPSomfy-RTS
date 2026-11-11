# Remaining stack qualification

[Issue #33](https://github.com/SidShaytay/ESPSomfy-RTS/issues/33) tracks this follow-up to the [completed bounded audit](firmware-stacks.md). Task stack allocations remain unchanged. The baseline does not qualify active RF, full updater TLS/download, local editing, deliberate Wi-Fi reconnects or long recovery.

## Repeatable controls

The USB port normally emits diagnostics only. `bash tools/audit-stacks.sh instrumented` adds measurement; `bash tools/audit-stacks.sh controlled` additionally enables `ESPSOMFY_AUDIT_CONTROL`. Controlled firmware is for an explicitly authorized test session. Ordinary and measurement-only images omit the command receiver and download-only audit API. The build checks the emitted measurement/control markers before accepting a result.

Commands are newline-terminated ASCII: `AUDIT <increasing uint32 sequence> <command>`. The parser bounds lines to63 bytes, drains invalid lines, rejects replayed accepted sequence IDs and does not echo input. One line/64 bytes maximum is processed per loop pass. Unsupported commands cannot install firmware or issue arbitrary RF. Responses contain only numeric fields and fixed role names; raw surrounding device logs remain private.

Commands: `state`, `turn -1`, `turn 1`, `down`, `up`, `back-down`, `back-up`, `release`, `wifi-reconnect`, `update-check`, `update-stage`, `update-cancel`. Button/turn inputs pass through the production UI debounce, visibility/presentation, authorization and service admission paths. Injected held inputs expire after two seconds; `release` restores physical inputs. There is no credential injection or unlock bypass. UI navigation can reach real actions, so scripts must check the expected screen/selection before activation and cancel drafts rather than save.

Reconnect and updater commands require the current authorized, unfrozen local session. Reconnect preserves radio mode and saved credentials and is limited to once per30 seconds. Check uses the production local updater adapter. Stage holds the ordinary exclusive preparation owner and loop admission, refreshes the exact candidate identity, uses the production TLS transport and complete image verifier, then frees locally owned PSRAM. It never enters flash admission, selects a boot slot or restarts. An equal/older compatible published image can be downloaded for this measurement; normal newer-version installation eligibility is unchanged. A prepared status here records verification, not retained installable bytes.

`tools/audit_console.py` is a standard-library USB client. It claims the port exclusively, clears HUPCL, sends no DTR/RTS ioctls, creates a private capture without overwriting an existing file, and makes one command attempt. Missing acknowledgements are ambiguous and must not be replayed. Stop the existing passive collector before opening this client; restore it afterward. Merely connecting sends no command. Separate invocations must continue sequence IDs within a boot. Keep a single connection for a scripted run.

## Additional stack observations

The instrumented loop initializes bounded SDK measurements after normal startup. Idle0, idle1 and the FreeRTOS timer daemon have scheduler-lifetime handles; one foreign scan every five seconds rotates through them. System-events, TCP/IP and ESP timer tasks self-sample through supported callbacks. TCP/IP submission is nonblocking with one outstanding request. SDK callbacks defer numeric records to loop, avoiding UART logging on those tasks. No global task snapshot, scheduler suspension or dynamic Wi-Fi handle lookup is used.

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

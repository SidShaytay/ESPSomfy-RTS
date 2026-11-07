# Issue #15 verification and hardware approval plan

## Offline review

The implementation is on `codex/issue-15-display`, based on verified standard-layout revision `7a8e529`. Review covered the snapshot boundary, failure paths, SPI transaction lifetime, GPIO reservations, memory and refresh bounds. It caught and fixed nested radio transactions during `Init()`, plus an old GPIO test fixture that used the newly reserved display DC pin. No new actuation path, credential display, device access or upstream submission is included.

The status model and driver contain no command API. The adapter reads only Wi-Fi/listener state, the radio's locked availability/frequency snapshot and the firmware build version. No network endpoint, persistent setting, shade model, pairing/counter logic or HA protocol changes are introduced. Radio readiness checks remain intact. Shared-SPI transaction settings change in the vendored driver, including headless/generic builds, so those builds and the actual driver harness are part of validation.

The Adafruit dependency is confined to the LCD environment, uses its BSD-licensed GFX canvas/font, and is pinned along with its BusIO dependency. No GFX SPI/display driver is instantiated. The panel has no readback channel: software failure injection establishes recovery from detectable failures, not physical panel detection.

## Offline results — November 6, 2026

- Native suite: 642 passed, one existing optional private-backup fixture skipped (no `CONFIG_BACKUP` provided).
- JavaScript: 62 passed. Release-version script passed. Manifest tooling: 24 tests passed.
- Board profile/reserved-output tests and non-S3 profile rejection passed. Panel/SPI harnesses passed; the original radio driver fails the clock/transaction regression as expected. Detectable SPI initialization failure is also tested and keeps brightness off.
- LCD, headless Plus and generic ESP32 firmware builds passed. The only compiler warning is the inherited volatile increment in `Transceiver::handleReceive`.

| Build | Static RAM | Firmware image | OTA slot |
| --- | ---: | ---: | ---: |
| Plus LCD | 105,124 bytes | 1,341,054 bytes | 3,342,336 bytes |
| Plus headless | 102,508 bytes | 1,290,503 bytes | 3,342,336 bytes |
| Generic ESP32 | 103,044 bytes | 1,369,550 bytes | 1,572,864 bytes |

LCD static RAM grows by 2,616 bytes, with an additional 720-byte heap canvas allocated before setup. This is linker/allocation accounting, not measured hardware heap/PSRAM usage. PSRAM is not explicitly requested by the renderer. Actual rendering/lock latency and radio response are unmeasured; 4.608 ms is only the calculated pixel wire time.

This workstation used the existing `rocm-7.2.3` container for native C++ checks because host GCC lacks the system development headers. Firmware builds used the existing pinned pioarduino tool cache on the host with `PLATFORMIO_BUILD_DIR=build/pio`, keeping container and host build products separate. Local firmware/ELF/partition files are under `build/pio/<environment>/`. Sanitized logs and candidate hashes are retained under `build/verification/` (ignored by Git). The display guide shows PlatformIO's default output location for a normal build.

## Hardware procedure — approval required before step 1

[Issue #15](https://github.com/SidShaytay/ESPSomfy-RTS/issues/15) requires a concrete reviewed plan and Sid's approval before flashing or operating the working device. Nothing below has been executed. Approval for this procedure must explicitly cover the firmware update/reboots, read-only observations and Wi-Fi interruption; it does not authorize RF, motion, pairing or limit changes.

1. Agree on a quiet maintenance window. Identify and temporarily suspend HA/HomeKit/other automation writers so they cannot submit commands during testing; record their prior state. Do not run another radio with the same identities. Capture fresh portable export, current protocol/settings/counters, NVS and authoritative configuration privately using the established recovery process. Confirm the running layout is still the standard layout installed on November 6; if anything differs, stop and revise the plan.
2. Retain checksummed current headless firmware/ELF, exact running partition table, fresh state and the tested recovery tools outside Git/shared storage. Compare display/headless `partitions.bin` byte for byte and confirm hashes of the candidate and recovery firmware. The proposed installation is firmware-only into the inactive OTA slot through the existing authenticated local update path. No filesystem, bootloader, partition-table, NVS, export import, factory image or whole-chip erase is part of this procedure. If the existing update path cannot safely do that, stop rather than substituting a flash command.
3. Install the display firmware and permit its update reboot. Keep USB diagnostics available. Check BOOT/RUN, build identity and URL. Confirm register validation and RX-entry checks pass with the LCD active; compare read-only web/protocol configuration, shade/group identities, calibration and counters against the fresh snapshot. Verify HA and the existing HomeKit integration remain connected using status/availability only. Do not actuate covers to check connectivity.
4. Observe for ten minutes with the screen active, dimmed and off; wake periodically with the encoder button. Rotate the encoder and test held/bouncing presses. Record USB memory/timing metrics, smallest internal free heap, PSRAM free and watchdog/reset behavior. Confirm no counter or shade/programming-queue changes attributable to display actions. If an unexpected writer is found, stop and restore maintenance controls before continuing.
5. With approval for a brief network interruption, block only this controller's AP association temporarily; avoid changing its stored credentials. Verify disconnected/connecting states remove the old URL, then restore the association and verify the new live address and listener. If AP fallback naturally occurs, verify its address. Stop if it risks losing the authenticated update/recovery route. Service/radio failure injection stays offline; do not disable/reconfigure the working radio solely to make an LCD error appear.
6. Measure maximum row rendering and bus-lock time during wake/reconnection; target under 10 ms per row/lock, no watchdog failures or unbounded heap loss. Use a logic analyzer on SPI/RX, if available, to compare received pulse/processing latency against the headless baseline while updating and waking. Confirm nonoverlapping CS and proper clock/mode transitions. **No RF stimulus or remote-button/motor command is authorized by this plan.** If ambient RX activity provides insufficient evidence, record radio responsiveness as pending and request separate approval for a controlled receiver bench test with no paired motor identities. Do not claim a compile or register check proves RF responsiveness.
7. If readability, SPI timings, readiness, web/HA continuity or memory fail, stop and return to the retained headless firmware through firmware-only OTA if reachable. If not reachable, use the established approved USB recovery path to restore only firmware appropriate to the current layout. Stop for a revised plan if that would require partition/NVS/filesystem writes. Never restore stale counters or a whole raw backup as an automatic rollback. Confirm read-only settings and fresh counters after rollback.
8. Restore suspended automations to their recorded state only after the controller is stable. Preserve sanitized results, firmware hashes, timings and comparison outcomes; keep raw captures private. Mark hardware acceptance only with actual evidence. Upstream publication and phase-2 shutter controls remain separate approvals.

## Acceptance status

Wiring/reference review, optional module/build configuration, offline transition/failure tests, command-path isolation and both build variants are implemented. Measured physical SRAM/PSRAM use, rendering latency, shared-SPI/RF responsiveness, LCD readability/brightness, failure recovery on the installed device and web/HA continuity remain pending the approved procedure. Issue #15 must stay open until that evidence is recorded.

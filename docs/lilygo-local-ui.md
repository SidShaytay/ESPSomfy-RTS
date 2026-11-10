# Local controller UI

Use [the standard flashing procedure](flashing.md) for backup and firmware-only A/B updates. Build `lilygo-t-embed-cc1101` for either CC1101 or Plus; the shared image includes display, UI and commands.

The 320×170 UI adds status, room selection, controls, details and saved display settings to the LILYGO CC1101/Plus display. It follows the approved dark/teal mockup using the real Adafruit fonts and vector drawing; the generated previews use the same renderer as the board.

The offline implementation now browses configured rooms, devices and groups; it no longer depends on fixed target IDs. Issue #25's administration and updater integration remains in progress in the [capability matrix](lcd-web-capability-matrix.md). The dated installation evidence below describes older installed builds and does not authorize a new live stage.

## Use the encoder

- First input after dim/off wakes only. A button held during boot is ignored until released.
- From Status, turn or press to open Home. Hold for Details; turn there to scroll all fourteen diagnostic rows. Details distinguishes compiled Firmware from installed web-file Application version. Press or Back returns to Status.
- On Home, turn to Rooms, Devices, Groups, Settings, Display, Status or Administration, then press. Rooms includes All rooms and Unassigned filters. Catalogues show three rows at a time, keep the selected device ID when entries reorder, and handle every configured device/group. Names can repeat; the visible stable ID identifies each target.
- **Administration → Rooms / Devices / Groups** adds, edits and deletes configuration through protected local forms. Apply changes only the draft; Save performs the edit. Device/group Save and all Delete confirmations start with Cancel selected. Full lists show every stable ID; Add is marked full when no slots remain. If an edit applies but its model file is not saved, Retry saves the current file without repeating the edit. See [entity administration](lcd-entity-administration.md).
- On a control page, turn to the available UP/STOP/DOWN or Toggle action and press. More opens additional supported actions: position, tilt, MY position, 80-bit Favorite, steps and sun flags. The menu reflects the actual device capabilities; it adds no separate light opcode. Group actions require compatible configured RF members. An 80-bit group also offers Favorite when every configured member supports it; mixed, missing or incompatible members disable it. This sends one command using the group identity and updates member estimates, without substituting individual member commands.
- Numeric position/tilt drafts range from 0 to 100; steps range from 1 to 127. Turn to adjust, press to review, then turn from the default **Cancel** to **Apply** and press. Turns and Cancel send nothing. An unavailable position estimate prevents opening a position draft rather than choosing a target silently. Back cancels a draft and returns directly to the main control page with STOP selected when supported.
- Push requires the selected row, target, action and Apply choice to have reached the panel. If SPI is busy, wait for the display and press again. A previously shown STOP remains reachable while the same target repaints estimates. Changed identity, configuration or group membership requires reopening the target.
- Back or a long hold returns without repeating a command. Display drafts are cancelled by Back. Press advances through Brightness, Dim after and Sleep after; pressing the last row explicitly saves. Saves occur together, never on every turn, and failures are shown.
- Brightness levels are 2, 4, 8, 16, 32, 64, 128 and 255 out of 255 (default 32). Dim/sleep choices are disabled, 5, 10, 15, 20, 30, 60, 120 and 300 seconds. **No dim** disables dimming; **No sleep** keeps the display on. Defaults are dim after 10 seconds and sleep after 20 seconds, measured independently from the last input.
- While any configured shade is estimated moving, the UI stays awake so STOP remains reachable. Wake/rotation/holding/Back alone creates no shutter request. With No sleep selected, idle navigation returns to Status after 30 seconds while leaving the display on.

An accepted button press first enters the shared model-loop queue. While it is still pending, the display says **Queued - Back cancels**. Back or hold retracts that unsubmitted local action; an unsubmitted action expires after five seconds and never retries automatically. Changed configuration, authorization, restore history or a backward clock also refuses the old intent.

A fresh STOP for the same unchanged target retracts its queued UP/DOWN/other action. If the target is already idle, this only cancels the queued start and sends no MY. If moving, STOP joins the end of the shared queue. **Cancelled; STOP not queued** means the old start was removed but the queue had no room for the replacement; press again when ready. Other targets' pending actions require explicit Back cancellation. Back cannot undo a command already accepted by the existing request service.

The current font displays ASCII. Unsupported UTF-8 code points and control characters appear as `?`, while stored names remain unchanged. Stable IDs distinguish labels that render identically. Separate/Euro GPIO relay tilt is unavailable pending an explicit actuator design; ordinary lift and existing GPIO remote tilt remain available where supported.

Configured web credentials also unlock local controls and administration. After two minutes without input, new actions require unlock. Only an already authorized, continuing movement retains ordinary STOP, for at most 30 seconds after the original expiry. Idle, changed configuration, explicit lock or sleep ends that exception; later movement does not regain it. Tilt STOP, MY, Toggle and every other action require unlock after expiry. Numeric drafts discard on timeout, authorization replacement or target changes. See the [interaction and security policy](lcd-interaction-security-design.md) for attempt limits and screenshot protection.

The [manufacturer schematic](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/master/hardware/T-Embed-CC1101%20V1.0%2024-07-29.pdf) and [component/pin list](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/master/README.md) do not document an ambient-light sensor. Brightness is manual. The encoder uses INA 4, INB 5 and KEY 0; Back uses GPIO6. The decoder follows the manufacturer's TWO03 mode, recognizing detents at both 00 and 11 and rejecting invalid Gray-code transitions/bounce. Sid confirmed the display, wheel and separate Back button work on the installed update.

The Wi-Fi glyph indicates connection, not measured signal strength. The installed build reads battery percentage and voltage from the BQ27220 fuel gauge. Status shows percentage; Details shows percentage and voltage. Startup, no battery, invalid values and failed reads are labeled explicitly. USB/charging detection remains outside this change. Live checks returned 100% and 4,146 mV; these are gauge-reported values, not an independent capacity measurement.

## If a settings save fails

A settings save error leaves the running settings unchanged and does not automatically reconnect or restart the controller. If the error says settings may be partly persisted, retry the complete intended settings before restarting: some stored values may already have changed, and a restart can load those values. Reading settings shows the running values, so it cannot prove that storage rolled back.

This shared settings service is groundwork for issue #25's local administration screens; it does not mean those menus are already implemented. Backup/restore and manual firmware/web-file transfers remain browser-owned. The existing restore workflow has separate completion and partial-write limitations; this change does not make restores transactional.

## Health in Details

Hold on Status, then turn to the bottom four Details rows. They show free internal memory and its boot low-water mark, the largest available internal block, free PSRAM, and the last published loop rate and maximum pass time. Memory uses KiB; timing uses milliseconds, with seconds for long passes. Very high rates use `k/s` (thousands per second). Details refreshes once per second; loop statistics use the existing published window, normally every 30 seconds. `Loop: awaiting sample` means no window has been published yet. Missing PSRAM is labeled separately from a present device with zero free memory. These are read-only diagnostics.

## Memory and rendering

| Allocation | Location | Size | Lifetime |
| --- | --- | ---: | --- |
| Full 320×170 RGB565 framebuffer | Explicit `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` | 108,800 bytes | UI lifetime |
| Optional immutable screenshot | Same explicit PSRAM allocation | 108,866 bytes | One HTTP response; freed on completion/disconnect |
| Four-scanline SPI bounce buffer | Internal static RAM | 2,560 bytes | UI lifetime |
| Single pending local command with exact guard | Internal static RAM | At most 704 bytes | Model-loop lifetime |
| Shared 32-item queue payload (all entry points) | Internal RTOS heap | At most 2,560 bytes plus RTOS bookkeeping | Application lifetime |
| Simple fallback text canvas | Small heap allocation | 720 bytes | Display lifetime |
| Fallback/clear pixel scanline | Internal static RAM | 640 bytes | Display lifetime |

The main drawing buffer uses about 1.3% of the 8 MiB PSRAM. With one screenshot in flight, pixel storage peaks around 2.6%. Font bitmaps and drawing code reside in flash. Navigation/model values, tiny band hashes, driver state and normal network bookkeeping remain in internal memory; they do not justify using a second internal framebuffer.

The graphics allocation never silently falls back to internal SRAM. If PSRAM is absent or the allocation fails, the existing simple status renderer shows `LCD: PSRAM not found` or `LCD: PSRAM alloc failed`, with the same saved dim/sleep timeouts and awake brightness. If its small text buffer, PWM or SPI initialization also fails, the backlight remains off and web/HA continue. Failure of the fallback text allocation alone does not disable a working PSRAM renderer.

Rendering happens only when visible state changes. Frame contents live in PSRAM; changed four-line bands are copied/byte-swapped into the internal transfer buffer before trying the existing radio mutex. One band is attempted per two milliseconds, with zero lock wait; the last band contains two lines. The LCD path never remaps radio pins, changes SPI pin modes, masks interrupts or ends the shared bus. Hashes skip unchanged bands. No full-screen animation or double buffering is used.

Commands require the currently selected page/target/action to have been presented. Position/uptime repainting does not disable an already presented STOP selection. Fresh model validation checks the exact existing RF identity and group member identities before dispatch; there are no new rolling-code identities. GPIO4/5/6 are reserved against relay output assignments in the shared CC1101/Plus build.

The enabled build reuses `ShadeRequests` on the model-owning loop. Group UP/DOWN use the existing group identity. When all configured members are estimated moving, group STOP uses the same shared identity as UP/DOWN. A refused shared STOP has no individual fallback. For mixed groups, STOP addresses only moving eligible members individually, rechecking immediately before submission, to avoid broadcasting MY to idle members. Partial acceptance is reported explicitly. RTS has no motor position acknowledgement: estimates can be wrong, and a shared STOP/MY radio command cannot provide an absolute physical no-favourite guarantee. Actual RF/STOP semantics require separate approved testing. The shared canonical environment defines `ESPSOMFY_LOCAL_COMMANDS`; reduced-feature release configurations are retired.

## Build and inspect offline

```bash
~/.cache/espsomfy-lilygo/pio-python/bin/pio run -e lilygo-t-embed-cc1101
~/.cache/espsomfy-lilygo/pio-python/bin/pio test -e native
bash tools/preview-local-ui.sh
bash tools/test-status-panel.sh
bash tools/test-status-health.sh
bash tools/test-board-profile.sh
python3 -m unittest discover -s tools -p test_lcd_image.py
```

The preview tool compiles the production renderer with the actual pinned GFX library/fonts, produces PNGs at exactly 320×170 in `build/ui-preview/`, and runs runtime/capture harnesses. Set `GFX_DIR` if library dependencies are outside the default `.pio/libdeps/lilygo-t-embed-cc1101/Adafruit GFX Library` directory. Only Arduino/network/heap boundaries are stubbed; the rendering, navigation, frame-copy and capture logic are production code.

On this workstation, native checks use the existing `rocm-7.2.3` container. Host firmware builds set `PLATFORMIO_BUILD_DIR=build/pio` and `PLATFORMIO_LIBDEPS_DIR=build/libdeps` to keep their artifacts separate from container-created native folders. The packager validates the shared full-feature CC1101/Plus image. Simple status remains an allocation-failure fallback, rather than a separate release image.

## Capture the hardware framebuffer

After installing the UI build, the read-only endpoint is `GET /lcd/screenshot`. The existing HTTP authentication middleware applies: supply the existing `apikey` header when security is enabled. No token is put in the URL. The shared build registers this route; allocation fallback has no full framebuffer and returns 503.

```bash
python3 tools/capture-local-ui.py --host <controller-address>
# If secured:
python3 tools/capture-local-ui.py --host <controller-address> --apikey-file <private-key-file>
```

The helper stores a private PNG under `~/.local/state/espsomfy-lilygo/lcd-captures/`, or uses an explicit new `--output file.png` / `file.bmp`. It never wakes the display, flashes firmware or sends a shutter request, and refuses HTTP redirects to protect the key. Captures include only the already-visible pixels; store/share them with the same care as a photo of the display.

The board waits for a fully transferred frame, copies it on the main loop into an immutable PSRAM response buffer, and streams a top-down RGB565 BMP. Only one capture may be pending/in flight; another receives 429. Missing framebuffer, allocation failure or no completed frame within 1.5 seconds returns 503. Request continuations use the library's weak-pointer/pause API, so a disconnected waiter is not dereferenced. Response destruction frees its PSRAM copy.

`X-LCD-Frame`, `X-LCD-Backlight` and `X-LCD-Commands` identify the frame, PWM value and local command lock. PWM0 means the physical backlight is off even though a screenshot still shows the panel's stored pixels. A capture is evidence of pixels sent, not a camera observation of physical brightness, orientation, wiring or panel faults. Simple fallback mode has no full RGB framebuffer and returns 503; use a photo for that case.

See the installation evidence below for live memory/timing and regression results. Sid confirmed the display, wheel and Back button work. Saved display-setting persistence remains a manual acceptance check.

## Offline verification — November 6, 2026

- Native suite: 651 passed; one pre-existing optional private-backup fixture skipped.
- JavaScript suite: 62 passed. Release-version script and 24 manifest tests passed. PNG/BMP conversion: two tests passed.
- Real-font renderer generated eight exact-size screens with guard pixels intact. Runtime tests checked explicit PSRAM allocation, changed-band skipping, bounded transfers, a busy bus, command-locked navigation and allocation fallback. Capture tests checked completed-frame gating, immutable pixels, one in flight, timeout, disconnect and allocation/response failure.
- Board/reserved-GPIO and actual CC1101 SPI harnesses passed. The UI, original simple-status, headless Plus and generic ESP32 builds passed. The armed adapter was compile-checked offline in a separate temporary environment; that image was not installed.
- Final source candidate `1664cf1`: 108,044 bytes linker-reported static RAM, 1,359,642 bytes linker-reported flash usage and a 1,404,336-byte actual firmware image in a 3,342,336-byte OTA slot. The simple-status build reports 105,132 bytes static RAM, so the new static cost is 2,912 bytes; the large drawing allocation is external PSRAM, not included in that static figure.
- Review covered input bounds, held/release gestures, STOP/identity safeguards, model ownership, GPIO reservations, PSRAM failure policy, SPI transaction lifetime, HTTP access checks, request/response lifetimes and bounded capture memory. A review fix made Both rooms use the current model names for its separate estimates. The installed preview's disassembly independently showed its local dispatch function only returning false, with no calls.

The device still needs direct physical validation of encoder direction/feel, brightness/readability and the fallback screen under an induced hardware failure. No RF responsiveness, shutter movement, favourite, pairing or motor-limit test has been performed. The assistant must not initiate movement testing; Sid may operate the controls manually.


## Installed preview — November 6, 2026

Sid explicitly authorized board updates with a suitable backup and firmware-only A/B OTA, while withholding shutter movement testing. A fresh portable backup and all five settings sections/idle entity counters were captured privately immediately before installation. The candidate partition table matched the verified installed standard layout; the prior active phase-1 firmware/ELF was retained and its slot was not overwritten. A new full-flash dump was omitted under Sid's existing A/B-only exception. Only `/updateFirmware` was posted; no filesystem, partition-table, NVS restore, bootloader or factory image was uploaded.

Installed source: `1664cf1`, canonical `lilygo-t-embed-cc1101-plus-ui` preview. The exact binary SHA256 is `c08af3f96937d82c76203d2ef428d68b3ff1cb90746d3d38133375c7c8a83736`. Its compiled local dispatch function was disassembled before upload and contained only a false return, with no calls. The separate armed compile-check artifact was not used.

The new screenshot endpoint returned an actual 320×170 frame with `X-LCD-Commands: locked`. It showed the new dark/teal status layout, connected Wi-Fi, HTTP listening and radio ready at 433.42 MHz. Protocol state/settings matched across reboot: 28 emitted entity fields and all five settings sections showed no differences, including unchanged counters and idle directions. Web assets returned 200; HA's three covers were available and idle. No HA service or shutter/RF/programming request was sent.

The initial capture reported PWM 4 during dimming; a later idle capture reported PWM 0, confirming the new idle-off behavior without a wake operation. Three successive extra downloads succeeded, exercising capture-buffer release on the real server. While off, the screenshot correctly retains the panel's last transmitted pixels rather than pretending to measure physical brightness.

Nineteen periodic comparisons over 613 seconds passed without a restart; the final follow-up, including after closing the task-owned USB reader, reported 662.336 seconds of uninterrupted candidate uptime. Captured diagnostics reported the 108,800-byte PSRAM allocation with fallback 0, maximum full-frame paint 6,469 µs, maximum four-line transfer 1,508 µs and maximum radio/SPI lock 1,489 µs. Twenty samples in the observation window recorded minimum free internal SRAM 156,656 bytes and minimum free PSRAM 8,260,548 bytes, with no busy skips or crash/watchdog markers. These timings cover boot/status rendering and capture under an idle radio workload; they are not a measurement of RF responsiveness or every physical control page.

Sid was away from the device, so encoder direction/detent feel, physical color/readability, manual brightness/persistence, induced fallback behavior and on-air STOP/MY semantics remain pending. No motion testing or upstream publication occurred. This installation was later superseded by the enabled-controls update below. Private backups, screenshots, scripts, checksums and logs are selected by `~/.local/state/espsomfy-lilygo/LATEST_UI_INSTALL`; no raw configuration or actual hardware screenshots are committed.


## Installed input/settings update — November 6, 2026

Installed source `7061e98`, canonical UI environment, with manual controls enabled. Firmware SHA256: `5df30962d252eb7753984a6c30af7cf7901bb8668171681d8c13da7025891f6c`. This fixes wheel detent rate/direction, enables GPIO6 Back, and adds saved independent dim/sleep choices including No dim/No sleep.

A fresh portable controller backup and all five settings sections were saved before upload. The candidate partition table matched the verified installed layout; only the inactive firmware slot was updated through `/updateFirmware`. The previous preview binary/ELF and active rollback slot were retained. No new full-flash dump was required under Sid's firmware-only A/B authorization.

The live framebuffer returned `X-LCD-Commands: enabled`. Read-only checks confirmed Wi-Fi connectivity, radio initialization at 433.42 MHz, identical entity state/counters and all five settings sections after reboot, and all three Home Assistant covers available and idle. The candidate ran beyond the 60-second network-up OTA confirmation window without a restart. USB diagnostics reported PSRAM framebuffer 108,800 bytes, fallback 0, maximum paint 6,470 µs and maximum SPI lock 1,491 µs. No shutter command or HA service was sent by the assistant.

Verification: 655 native tests passed with one pre-existing optional private-backup skip; real-GFX renderer/runtime/capture and GPIO/SPI harnesses passed. UI, simple-status and headless builds passed. Physical wheel feel, Back, display-setting persistence and on-air control behavior await Sid's manual validation. Private installation evidence is selected by `~/.local/state/espsomfy-lilygo/LATEST_UI_INPUT_FIX`.


## Command-category fix

Sid confirmed that display, wheel and Back work, but screen commands did not move the shutters while web controls worked. The adapter incorrectly required `shade_types::roller`; both configured devices are `shade_types::shutter` (4), so they and their group were marked invalid before dispatch. The UI now leaves category handling to the existing shared dispatcher.

A regression harness compiles the production adapter with an in-memory model and recording request sink. It reproduced the failure with the actual shutter category, then passed after the fix. It covers every defined device category, individual/group requests, stale identities and idle/mixed-group STOP behavior without linking any radio implementation. Run `bash tools/test-local-ui-adapter.sh`; this also runs with the existing preview harness.

The enabled-controls UI firmware builds successfully; 655 native tests passed with the existing optional fixture skip, and renderer/runtime/capture/adapter harnesses passed. The fix was first held offline while another agent used the board, then included in that agent’s installed group-control firmware and in the cumulative update below.


## Battery percentage

The [LILYGO schematic](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/master/hardware/T-Embed-CC1101%20V1.0%2024-07-29.pdf) specifies SDA8/SCL18 and BQ27220 address 0x55. The [TI technical reference](https://www.ti.com/lit/ug/sluubd4a/sluubd4a.pdf) documents the direct SOC, voltage, current, battery-presence and initialization registers. The display uses the gauge-reported percentage, never a voltage-to-percent guess.

The reader uses I2C controller 1 at 100 kHz with a 2 ms transaction timeout, one register read per loop pass separated by at least 10 ms. A five-register sample is attempted every ten seconds. It writes only register pointers for reads: no charger settings, gauge profiles, battery capacity, calibration, reset or control subcommands are changed. SDA8/SCL18 are reserved against relay assignments. A failed read clears the prior sample instead of presenting stale percentages as fresh. Missing battery and incomplete/config-update states do not produce percentages.

Host tests cover byte order/short reads/NACK, bounded polling and clock wrap, battery presence, initialization/config-update, invalid percentages, zero charge, voltage/current, failed-read recovery and display messages. Inverting the percentage validity condition caused four tests to fail. All 662 native tests passed with the existing optional fixture skip; renderer/runtime/capture/adapter and panel/radio/GPIO harnesses passed. Enabled UI, simple-status and headless Plus builds passed. The initial checks were offline while the device was reserved for another agent. The later hardware installation below confirms gauge communication and displayed percentage; capacity accuracy and I2C timing under active radio transmission remain unverified. No movement testing was performed.


## Installed versions/battery update — November 6, 2026

Sid returned the board and explicitly authorized flashing. Deployment source `b1ad2e4` combines feature source `ce5f559` with the already-installed issue-23 source, preserving its protocol/group feature rather than reverting it. This does not validate issue-23 movement behavior or merge unfinished group work into main.

A fresh complete 16 MiB flash image, portable export and all five settings sections were saved privately. Verification checked partition MD5, both application image checksums/SHA256, OTA selection CRC, active NVS CRCs, extracted authoritative configuration and current counters. The active source image matched the previous installed binary. After the USB read, the standard reset left the board offline; explicitly deasserting DTR and resetting restored the same application without a flash write. A fresh live capture then matched the verified backup.

Only the checked `firmware.bin` was posted to `/updateFirmware`; the inactive app0 slot was used and app1 retained. No filesystem/NVS/bootloader/table image or restore was uploaded. The new USB boot fingerprint matched the candidate’s embedded ELF SHA256, and the live framebuffer returned `X-LCD-Commands: enabled`. Battery diagnostics reported state ready, 100%, 4,146 mV and 0 mA; the actual status framebuffer showed Battery 100%. The installed web filesystem’s application version remains 4.0.1, separately displayed from compiled firmware v4.0.1 in Details.

Read-only verification found connected Wi-Fi, radio initialized at 433.42 MHz, operation 55/groupTarget still advertised, unchanged entity configuration/counters and all five settings sections, and all three HA covers available and idle. At 118.151 seconds uptime, settings/counters still matched, no restart/crash/OTA confirmation error appeared, and the task-owned USB reader was stopped. Combined checks passed: 706 native tests plus one optional skip, 63 JavaScript tests, real renderer/runtime/capture/adapter and panel/radio/GPIO harnesses, and the enabled UI firmware build. No assistant shutter commands or HA services were sent. Private evidence is selected by `~/.local/state/espsomfy-lilygo/LATEST_UI_DETAILS_POWER_INSTALL`.

See [System menu](lcd-system.md) for confirmed restart and browser file guidance.

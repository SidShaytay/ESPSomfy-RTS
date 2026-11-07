# Local shutter UI

The optional 320×170 UI adds status, room selection, controls, details and saved display settings to the LILYGO Plus display. It follows the approved dark/teal mockup using the real Adafruit fonts and vector drawing; the generated previews use the same renderer as the board.

**Physical shutter controls are enabled in the installed firmware.** Sid authorized manual operation and firmware installation; the assistant has sent no shutter commands. No upstream publication is included.

## Use the encoder

- First input after dim/off wakes only. A button held during boot is ignored until released.
- From Status, turn or press to choose an item. Hold for Details. Details lists Firmware (the compiled firmware version) and Application (the installed web-filesystem version) separately. Turn the wheel to scroll through all ten diagnostic rows; press or Back returns to Status. Missing version data is shown as unavailable.
- Turn clockwise to move down through the list and anticlockwise to move up, one item per physical detent. Choose an individual shade, Both rooms, Status or Display settings.
- Press opens the selected page. Hold returns; the separate Back button also returns. Back saves any display-setting changes.
- On Controls, turn to highlight UP/STOP/DOWN and press to submit the action. STOP is selected initially and after an attempt. A long hold goes back and never repeats a command.
- Positions are estimates. Both rooms shows the current names and separate estimates for its members; group selection has an amber double-shutter icon.
- In Display settings, turn to adjust the highlighted value. Press advances through Brightness, Dim after and Sleep after; pressing on the last row saves and returns. Back or hold also saves and returns. Changes are saved together when leaving, rather than writing flash on every turn. A failure to save is shown explicitly.
- Brightness levels are 2, 4, 8, 16, 32, 64, 128 and 255 out of 255 (default 32). Dim/sleep choices are disabled, 5, 10, 15, 20, 30, 60, 120 and 300 seconds. **No dim** disables dimming; **No sleep** keeps the display on. Defaults are dim after 10 seconds and sleep after 20 seconds, measured independently from the last activity.
- While any configured shade is estimated moving, the UI stays awake so STOP remains reachable. Wake/rotation/holding/Back alone never creates a shutter request. With No sleep selected, idle navigation returns to Status after 30 seconds while leaving the display on.

The [manufacturer schematic](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/master/hardware/T-Embed-CC1101%20V1.0%202024-07-29.pdf) and [component/pin list](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/master/README.md) do not document an ambient-light sensor. Brightness is manual. The encoder uses INA 4, INB 5 and KEY 0; Back uses GPIO6. The decoder follows the manufacturer's TWO03 mode, recognizing detents at both 00 and 11 and rejecting invalid Gray-code transitions/bounce. Sid confirmed the display, wheel and separate Back button work on the installed update.

The Wi-Fi glyph indicates connection, not measured signal strength. Power readings remain unimplemented, rather than reporting fabricated USB/charging/battery information. They appear as unavailable on Details. The BQ power hardware needs separate read-only integration/validation before adding telemetry.

## Memory and rendering

| Allocation | Location | Size | Lifetime |
| --- | --- | ---: | --- |
| Full 320×170 RGB565 framebuffer | Explicit `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` | 108,800 bytes | UI lifetime |
| Optional immutable screenshot | Same explicit PSRAM allocation | 108,866 bytes | One HTTP response; freed on completion/disconnect |
| Four-scanline SPI bounce buffer | Internal static RAM | 2,560 bytes | UI lifetime |
| Simple fallback text canvas | Small heap allocation | 720 bytes | Display lifetime |
| Fallback/clear pixel scanline | Internal static RAM | 640 bytes | Display lifetime |

The main drawing buffer uses about 1.3% of the 8 MiB PSRAM. With one screenshot in flight, pixel storage peaks around 2.6%. Font bitmaps and drawing code reside in flash. Navigation/model values, tiny band hashes, driver state and normal network bookkeeping remain in internal memory; they do not justify using a second internal framebuffer.

The graphics allocation never silently falls back to internal SRAM. If PSRAM is absent or the allocation fails, the existing simple status renderer shows `LCD: PSRAM not found` or `LCD: PSRAM alloc failed`, with the same saved dim/sleep timeouts and awake brightness. If its small text buffer, PWM or SPI initialization also fails, the backlight remains off and web/HA continue. Failure of the fallback text allocation alone does not disable a working PSRAM renderer.

Rendering happens only when visible state changes. Frame contents live in PSRAM; changed four-line bands are copied/byte-swapped into the internal transfer buffer before trying the existing radio mutex. One band is attempted per two milliseconds, with zero lock wait; the last band contains two lines. The LCD path never remaps radio pins, changes SPI pin modes, masks interrupts or ends the shared bus. Hashes skip unchanged bands. No full-screen animation or double buffering is used.

Commands require the currently selected page/target/action to have been presented. Position/uptime repainting does not disable an already presented STOP selection. Fresh model validation checks the exact existing RF identity and group member identities before dispatch; there are no new rolling-code identities. GPIO4/5/6 are reserved against relay output assignments in every Plus build.

The enabled build reuses `ShadeRequests` on the model-owning loop. Group UP/DOWN use the existing group identity. Group STOP addresses only members estimated moving, rechecking each immediately before submission, to avoid broadcasting MY to idle members. RTS has no motor position acknowledgement: estimates can be wrong, and a shared STOP/MY radio command cannot provide an absolute physical no-favourite guarantee. Actual RF/STOP semantics require separate approved testing. The canonical `lilygo-t-embed-cc1101-plus-ui` environment defines `ESPSOMFY_LOCAL_COMMANDS`. Use the separate `lilygo-t-embed-cc1101-plus-ui-preview` environment when a command-locked build is wanted.

## Build and inspect offline

```bash
~/.cache/espsomfy-lilygo/pio-python/bin/pio run -e lilygo-t-embed-cc1101-plus-ui
~/.cache/espsomfy-lilygo/pio-python/bin/pio test -e native
bash tools/preview-local-ui.sh
bash tools/test-status-panel.sh
bash tools/test-board-profile.sh
python3 -m unittest discover -s tools -p test_lcd_image.py
```

The preview tool compiles the production renderer with the actual pinned GFX library/fonts, produces PNGs at exactly 320×170 in `build/ui-preview/`, and runs runtime/capture harnesses. Set `GFX_DIR` if library dependencies are outside the default `.pio/libdeps/lilygo-t-embed-cc1101-plus-ui/Adafruit GFX Library` directory. Only Arduino/network/heap boundaries are stubbed; the rendering, navigation, frame-copy and capture logic are production code.

On this workstation, native checks use the existing `rocm-7.2.3` container. Host firmware builds set `PLATFORMIO_BUILD_DIR=build/pio` and `PLATFORMIO_LIBDEPS_DIR=build/libdeps` to keep their artifacts separate from container-created native folders. Do not use the headless artifact packager to label this UI firmware. Headless and the original simple-status environments remain available.

## Capture the hardware framebuffer

After installing the UI build, the read-only endpoint is `GET /lcd/screenshot`. The existing HTTP authentication middleware applies: supply the existing `apikey` header when security is enabled. No token is put in the URL. The route is absent in headless/simple-status builds.

```bash
python3 tools/capture-local-ui.py --host <controller-address>
# If secured:
python3 tools/capture-local-ui.py --host <controller-address> --apikey-file <private-key-file>
```

The helper stores a private PNG under `~/.local/state/espsomfy-lilygo/lcd-captures/`, or uses an explicit new `--output file.png` / `file.bmp`. It never wakes the display, flashes firmware or sends a shutter request, and refuses HTTP redirects to protect the key. Captures include only the already-visible pixels; store/share them with the same care as a photo of the display.

The board waits for a fully transferred frame, copies it on the main loop into an immutable PSRAM response buffer, and streams a top-down RGB565 BMP. Only one capture may be pending/in flight; another receives 429. Missing framebuffer, allocation failure or no completed frame within 1.5 seconds returns 503. Request continuations use the library's weak-pointer/pause API, so a disconnected waiter is not dereferenced. Response destruction frees its PSRAM copy.

`X-LCD-Frame`, `X-LCD-Backlight` and `X-LCD-Commands` identify the frame, PWM value and local command lock. PWM0 means the physical backlight is off even though a screenshot still shows the panel's stored pixels. A capture is evidence of pixels sent, not a camera observation of physical brightness, orientation, wiring or panel faults. Simple fallback mode has no full RGB framebuffer and returns 503; use a photo for that case.

See the installation evidence below for live memory/timing and regression results. Physical validation of the revised wheel/Back behavior and display settings remains pending.

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


## Command-category fix — built offline, not installed

Sid confirmed that display, wheel and Back work, but screen commands did not move the shutters while web controls worked. The adapter incorrectly required `shade_types::roller`; both configured devices are `shade_types::shutter` (4), so they and their group were marked invalid before dispatch. The UI now leaves category handling to the existing shared dispatcher.

A regression harness compiles the production adapter with an in-memory model and recording request sink. It reproduced the failure with the actual shutter category, then passed after the fix. It covers every defined device category, individual/group requests, stale identities and idle/mixed-group STOP behavior without linking any radio implementation. Run `bash tools/test-local-ui-adapter.sh`; this also runs with the existing preview harness.

The enabled-controls UI firmware builds successfully; 655 native tests passed with the existing optional fixture skip, and renderer/runtime/capture/adapter harnesses passed. The build has **not** been flashed: Sid reserved the device for another agent. Do not access or update the board until Sid returns it; collect fresh state and recheck its current firmware/layout before any later installation.

# Local shutter UI preview

The optional 320×170 UI adds status, room selection, controls, details and saved brightness to the LILYGO Plus display. It follows the approved dark/teal mockup using the real Adafruit fonts and vector drawing; the generated previews use the same renderer as the board.

**The installed preview cannot send local shutter commands.** Sid authorized firmware updates and display/status checks, but not shutter movement testing. Web/HA controls retain their existing behavior. No upstream publication is included.

## Use the encoder

- First input after dim/off wakes only. A button held during boot is ignored until released.
- From Status, turn or press to choose an item. Hold for Details (build, power telemetry availability and service diagnostics).
- Turn through the two individual shades, Both rooms, Status and Brightness. Press opens the selected page; hold returns.
- On Controls, turn to highlight UP/STOP/DOWN. A short press attempts the action; this build shows `Preview only - no commands`. STOP is selected initially and after an attempt. A long hold goes back and never repeats a command.
- Positions are estimates. Both rooms shows the current names and separate estimates for its members; group selection has an amber double-shutter icon.
- On Brightness, turn to change the awake level immediately, then press or hold to save and return. Levels are 2, 4, 8, 16, 32, 64, 128 and 255 out of 255 (default 32). The selected setting persists in the separate `LCDUI/brightness` NVS key; encoder turns do not write flash. A failure to save is shown explicitly.
- At ten seconds idle the screen dims; at twenty seconds it turns off and returns to Status. While any configured shade is estimated moving, the UI stays awake so STOP remains reachable in a future enabled build. Wake/rotation/holding alone never creates a command request.

The [manufacturer schematic](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/master/hardware/T-Embed-CC1101%20V1.0%2024-07-29.pdf) and [component/pin list](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/master/README.md) do not document an ambient-light sensor. Brightness is manual; no automatic light reading is inferred. The encoder uses INA 4, INB 5 and KEY 0 from that pin list. Its full-detent decoder rejects invalid Gray-code transitions and incomplete/bouncing steps. Physical direction/detent feel still needs user validation.

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

The graphics allocation never silently falls back to internal SRAM. If PSRAM is absent or the allocation fails, the existing simple status renderer shows `LCD: PSRAM not found` or `LCD: PSRAM alloc failed`, with the same 10/20-second bedroom timer and saved awake brightness. If its small text buffer, PWM or SPI initialization also fails, the backlight remains off and web/HA continue. Failure of the fallback text allocation alone does not disable a working PSRAM renderer.

Rendering happens only when visible state changes. Frame contents live in PSRAM; changed four-line bands are copied/byte-swapped into the internal transfer buffer before trying the existing radio mutex. One band is attempted per two milliseconds, with zero lock wait; the last band contains two lines. The LCD path never remaps radio pins, changes SPI pin modes, masks interrupts or ends the shared bus. Hashes skip unchanged bands. No full-screen animation or double buffering is used.

Commands require the currently selected page/target/action to have been presented. Position/uptime repainting does not disable an already presented STOP selection. Fresh model validation checks the exact existing RF identity and group member identities before dispatch; there are no new rolling-code identities. GPIO4/5 are reserved against relay output assignments in every Plus build.

An enabled build would reuse `ShadeRequests` on the model-owning loop. Group UP/DOWN use the existing group identity. Group STOP would address only members estimated moving, rechecking each immediately before submission, to avoid broadcasting MY to idle members. RTS has no motor position acknowledgement: estimates can be wrong, and a shared STOP/MY radio command cannot provide an absolute physical no-favourite guarantee. Actual RF/STOP semantics require separate approved testing. No enabled environment is offered by default; `ESPSOMFY_LOCAL_COMMANDS` is deliberately absent from the preview build.

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

See the installation evidence below for live memory/timing and regression results. Physical navigation and appearance remain pending while Sid is away from the device.

## Offline verification — November 6, 2026

- Native suite: 651 passed; one pre-existing optional private-backup fixture skipped.
- JavaScript suite: 62 passed. Release-version script and 24 manifest tests passed. PNG/BMP conversion: two tests passed.
- Real-font renderer generated eight exact-size screens with guard pixels intact. Runtime tests checked explicit PSRAM allocation, changed-band skipping, bounded transfers, a busy bus, command-locked navigation and allocation fallback. Capture tests checked completed-frame gating, immutable pixels, one in flight, timeout, disconnect and allocation/response failure.
- Board/reserved-GPIO and actual CC1101 SPI harnesses passed. The UI, original simple-status, headless Plus and generic ESP32 builds passed. The armed adapter was compile-checked offline in a separate temporary environment; that image was not installed.
- Final source candidate `1664cf1`: 108,044 bytes linker-reported static RAM, 1,359,642 bytes linker-reported flash usage and a 1,404,336-byte actual firmware image in a 3,342,336-byte OTA slot. The simple-status build reports 105,132 bytes static RAM, so the new static cost is 2,912 bytes; the large drawing allocation is external PSRAM, not included in that static figure.
- Review covered input bounds, held/release gestures, STOP/identity safeguards, model ownership, GPIO reservations, PSRAM failure policy, SPI transaction lifetime, HTTP access checks, request/response lifetimes and bounded capture memory. A review fix made Both rooms use the current model names for its separate estimates. The installed preview's disassembly independently showed its local dispatch function only returning false, with no calls.

The device still needs direct physical validation of encoder direction/feel, brightness/readability and the fallback screen under an induced hardware failure. No RF responsiveness, shutter movement, favourite, pairing or motor-limit test has been performed. Phase-2 movement testing needs Sid's explicit approval.


## Installed preview — November 6, 2026

Sid explicitly authorized board updates with a suitable backup and firmware-only A/B OTA, while withholding shutter movement testing. A fresh portable backup and all five settings sections/idle entity counters were captured privately immediately before installation. The candidate partition table matched the verified installed standard layout; the prior active phase-1 firmware/ELF was retained and its slot was not overwritten. A new full-flash dump was omitted under Sid's existing A/B-only exception. Only `/updateFirmware` was posted; no filesystem, partition-table, NVS restore, bootloader or factory image was uploaded.

Installed source: `1664cf1`, canonical `lilygo-t-embed-cc1101-plus-ui` preview. The exact binary SHA256 is `c08af3f96937d82c76203d2ef428d68b3ff1cb90746d3d38133375c7c8a83736`. Its compiled local dispatch function was disassembled before upload and contained only a false return, with no calls. The separate armed compile-check artifact was not used.

The new screenshot endpoint returned an actual 320×170 frame with `X-LCD-Commands: locked`. It showed the new dark/teal status layout, connected Wi-Fi, HTTP listening and radio ready at 433.42 MHz. Protocol state/settings matched across reboot: 28 emitted entity fields and all five settings sections showed no differences, including unchanged counters and idle directions. Web assets returned 200; HA's three covers were available and idle. No HA service or shutter/RF/programming request was sent.

The initial capture reported PWM 4 during dimming; a later idle capture reported PWM 0, confirming the new idle-off behavior without a wake operation. Three successive extra downloads succeeded, exercising capture-buffer release on the real server. While off, the screenshot correctly retains the panel's last transmitted pixels rather than pretending to measure physical brightness.

Nineteen periodic comparisons over 613 seconds passed without a restart; the final follow-up, including after closing the task-owned USB reader, reported 662.336 seconds of uninterrupted candidate uptime. Captured diagnostics reported the 108,800-byte PSRAM allocation with fallback 0, maximum full-frame paint 6,469 µs, maximum four-line transfer 1,508 µs and maximum radio/SPI lock 1,489 µs. Twenty samples in the observation window recorded minimum free internal SRAM 156,656 bytes and minimum free PSRAM 8,260,548 bytes, with no busy skips or crash/watchdog markers. These timings cover boot/status rendering and capture under an idle radio workload; they are not a measurement of RF responsiveness or every physical control page.

Sid was away from the device, so encoder direction/detent feel, physical color/readability, manual brightness/persistence, induced fallback behavior and on-air STOP/MY semantics remain pending. No motion testing or upstream publication occurred. The preview remains installed with local commands locked. Private backups, screenshots, scripts, checksums and logs are selected by `~/.local/state/espsomfy-lilygo/LATEST_UI_INSTALL`; no raw configuration or actual hardware screenshots are committed.

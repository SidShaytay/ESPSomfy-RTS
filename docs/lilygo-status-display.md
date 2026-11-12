# LILYGO status display and fallback

Both canonical LILYGO builds include the graphical local UI, display diagnostics and local shutter commands. Build `lilygo-t-embed-cc1101-plus` for the Plus or `lilygo-t-embed-cc1101` for the standard board; there are no separate headless, display-only, UI or preview environments. See the [local UI guide](lilygo-local-ui.md) for current navigation, saved brightness/dim/sleep settings, PSRAM rendering and screenshots.

## Build without connecting a device

Use the pioarduino tools installed by `bash tools/setup-lilygo.sh`:

```bash
bash tools/build-update.sh lilygo-t-embed-cc1101-plus
# For the standard board instead:
bash tools/build-update.sh lilygo-t-embed-cc1101
```

These commands build and validate artifacts without uploading, rebooting or opening a device. Both targets include `ESPSOMFY_STATUS_DISPLAY`, `ESPSOMFY_LOCAL_UI` and `ESPSOMFY_LOCAL_COMMANDS`, with pinned Adafruit GFX 1.12.1 / BusIO 1.17.4. Their physical-board identities remain separate. The implemented controls cover shade IDs 1 and 2 and group ID 1; the local UI does not replace every feature of the web interface.

The text renderer described below remains the fallback when a PSRAM framebuffer cannot be allocated. The [dated status-display validation record](lilygo-status-validation.md) describes the older status-only build and retains its original environment names and artifact identities as historical evidence.

## Read the fallback status screen

The landscape 320×170 page uses nine 18-pixel rows and text at twice the built-in font size:

- BOOT/RUN and uptime in hours/minutes.
- Battery percentage and voltage from the BQ27220 gauge, with explicit starting/no-battery/read-failure messages. The shared reader was checked on the installed interactive UI and reported 100% at 4,146 mV. USB and charging state are not inferred.
- Wi-Fi disconnected, connecting, connected, or access point; current IPv4 address when assigned. This reports local link state, without testing internet access.
- HTTP starting/listening/error, from the actual asynchronous TCP listener state. The `http://` address appears only with an assigned address and a listening server. Listening does not establish client reachability.
- Radio initializing/ready/unavailable and configured frequency. Ready reads the existing availability flag, which the Plus profile publishes only after register validation and successful RX entry. A busy snapshot reports unavailable until the next successful sample; the display never weakens the checks or probes/reconfigures the radio itself.
- Plus LCD/fork issue identity and the firmware's compiled version. The inherited source reports v4.0.1 even though the maintained baseline is pinned to the cjkas v4.0.2 revision; this feature does not relabel the firmware version.

The fallback follows the same saved brightness and dim/sleep preferences as the graphical UI (defaults: 32/255, dim after 10 seconds, off after 20 seconds). Press and release the encoder button (GPIO0) or Back to wake. The text-only fallback has no navigation or command dispatch. Do not hold GPIO0 while resetting/powering on: it is also the board's boot strap.

The text renderer itself submits no shutter/programming commands. Normal graphical navigation and local controls are implemented separately in `LocalUi`, which is compiled into both canonical builds. Passwords, SSIDs, tokens, identities and shade data are not part of the display snapshot.

## Wiring and shared SPI

Manufacturer references, checked November 6, 2026:

- [Plus product](https://lilygo.cc/products/t-embed-cc1101-plus): the CC1101 model with added nRF24L01, including its ST7789V LCD.
- [Pinned setup 214](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/c36826e0f91311cb591c4545fccf91e254fc55dc/lib/TFT_eSPI/User_Setups/Setup214_LilyGo_T_Embed_PN532.h): 170×320 panel, MOSI9, MISO10, SCK11, CS41, DC16, backlight21, reset=-1, inversion enabled.
- [Pinned initialization](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/c36826e0f91311cb591c4545fccf91e254fc55dc/lib/TFT_eSPI/TFT_Drivers/ST7789_Init.h) and [rotation](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/c36826e0f91311cb591c4545fccf91e254fc55dc/lib/TFT_eSPI/TFT_Drivers/ST7789_Rotation.h): RGB565, landscape MX|MV and row offset35, normal mode/inversion/power/gamma setup. The implementation uses software reset, then releases the bus during reset/sleep/display-on delays. It does not drive GPIO40 as a reset.

The manufacturer's TFT_eSPI HSPI recommendation addresses that library's direct register implementation. This module instead uses Arduino's shared `SPI` object with explicit transactions and the existing radio pins. It does not instantiate TFT_eSPI, remap pins, call `SPI.end()`, change SPI pin modes, or mask interrupts.

Each LCD transfer tries the existing recursive radio mutex with zero wait. A busy radio skips the transfer. CS41 is raised before transaction end; other slave selects stay deselected. The radio driver selects 4 MHz/mode0/MSB-first on every access so it cannot inherit the LCD's 20 MHz setting. The selected 4 MHz clock is below the limits in [TI’s SPI timing specification](https://www.ti.com/lit/ds/symlink/cc1101.pdf), including burst transfers without inserted delays. Reset ends its transaction before register helpers open theirs. Existing radio preflight may still end/reinitialize the bus under that same mutex; the display reopens it with the exact existing pins on its next transfer. GPIO16/DC and GPIO0/wake are reserved against saved/imported relay assignments in the canonical LILYGO profiles.

Snapshots are sampled once per second after setup. Only changed text rows transfer, at most one row per 20 ms. A row transfers 11,520 pixel bytes plus 11 command/address bytes; initialization clears two scanlines per pass. At 20 MHz the pixel wire time per text row is 4.608 ms, before CPU/driver overhead. This is a calculated bound on wire volume, not a hardware latency measurement. The first BOOT page has a bounded 1.2-second setup window; initialization has a five-second elapsed-time failure limit and never waits for the radio lock.

The text renderer uses a 720-byte monochrome row canvas and a 640-byte static pixel scanline and requests no PSRAM or DMA buffers itself. Normal graphical mode separately allocates its 108,800-byte framebuffer in PSRAM; see the [UI memory table](lilygo-local-ui.md#memory-and-rendering). Allocation/PWM/SPI initialization/initialization timeout failures leave the backlight off and let controller startup continue. The LCD is write-only: command completion cannot detect a disconnected or damaged panel; a dark panel must not be interpreted as controller failure.

Every 30 seconds USB diagnostics report free internal SRAM, free PSRAM, maximum complete row rendering time (`maxRowUs`), maximum SPI-lock duration (`maxBusUs`), and skipped row transfers. These measurements contain no configuration or shade data. Measured row rendering and bus-lock times are in the validation report. Sid’s photo confirms readable, correctly oriented text. Dim/off/wake behavior and on-air radio responsiveness still need direct confirmation.

## Offline verification

```bash
~/.cache/espsomfy-lilygo/pio-python/bin/pio test -e native
bash tools/test-status-panel.sh
bash tools/test-board-profile.sh
node --test 'test/js/*.test.js'
bash test/scripts/test_set_version.sh
python3 -m unittest discover -s tools -p 'test_lilygo_manifest.py'
```

Native tests cover status transitions, stale address/URL removal, AP mode, service/radio failure, pending/busy outputs, initialization failure, changed-row limits, dim/off/wake, held inputs and clock wrap. The hardware-driver harness compiles the production panel with only GPIO/SPI/font rasterization and heap telemetry stubbed. It asserts bus ownership, exact shared pins, chip-select order, command sequencing, byte counts, timeout, allocation/PWM failure and invalid row bounds. A separate harness compiles the actual CC1101 driver to check clock restoration and balanced transactions, including initialization. The pre-change driver fails that regression test. Both harnesses link without a shade dispatcher, command queue or radio transmission task.

See [verification and hardware approval plan](lilygo-status-validation.md) for results and the remaining acceptance steps.

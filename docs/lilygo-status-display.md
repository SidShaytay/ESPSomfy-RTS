# LILYGO status fallback

The shared CC1101/Plus build includes the interactive [local UI](lilygo-local-ui.md). This page describes its simple status fallback when PSRAM is unavailable. It is not a separate status-only production image. Dated physical results belong to the [historical validation record](lilygo-status-validation.md).

## Build for your physical board

Use pinned pioarduino Core 6.2.0:

```bash
~/.cache/espsomfy-lilygo/pio-python/bin/pio run -e lilygo-t-embed-cc1101
```

It requires display/UI/commands and pinned GFX 1.12.1 / BusIO 1.17.4; generic targets get no display dependencies. Keep the matching ELF. Build commands do not access a device. The packager in [LILYGO.md](../LILYGO.md) validates the exact supported CC1101/Plus hardware list and full feature set; Plus-only/suffix release names are retired.

## Read and wake the screen

The landscape 320×170 page uses nine 18-pixel rows and text at twice the built-in font size:

- BOOT/RUN and uptime in hours/minutes.
- Battery percentage and voltage from the BQ27220 gauge, with explicit starting/no-battery/read-failure messages. The shared reader was checked on the installed interactive UI and reported 100% at 4,146 mV. USB and charging state are not inferred.
- Wi-Fi disconnected, connecting, connected, or access point; current IPv4 address when assigned. This reports local link state, without testing internet access.
- HTTP starting/listening/error, from the actual asynchronous TCP listener state. The `http://` address appears only with an assigned address and a listening server. Listening does not establish client reachability.
- Radio initializing/ready/unavailable and configured frequency. Ready reads the existing availability flag, which the Plus profile publishes only after register validation and successful RX entry. A busy snapshot reports unavailable until the next successful sample; the display never weakens the checks or probes/reconfigures the radio itself.
- Actual CC1101/Plus build identity and the firmware's compiled version. The inherited source reports v4.0.1 even though the maintained baseline is pinned to the cjkas v4.0.2 revision; this feature does not relabel the firmware version.

The fallback uses saved display brightness and timeouts; defaults are awake brightness 32/255, dim after 10 seconds and off after 20 seconds. Press and release the encoder button (GPIO0) to wake; presses are debounced for 40 ms. Holding it cannot keep waking the display. Rotation has no effect. Do not hold GPIO0 while resetting/powering on: it is also the board's boot strap.

No initialization, rendering, failure, sleep, wake or rotation path submits shutter/programming commands. The fallback has no encoder navigation or local motor-control path; the normal interactive renderer provides those controls. Passwords, SSIDs, tokens, identities and shade data are not part of the display snapshot.

## Wiring and shared SPI

Manufacturer references, checked November 6, 2026:

- [Plus product](https://lilygo.cc/products/t-embed-cc1101-plus): the CC1101 model with added nRF24L01, including its ST7789V LCD.
- [Pinned setup 214](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/c36826e0f91311cb591c4545fccf91e254fc55dc/lib/TFT_eSPI/User_Setups/Setup214_LilyGo_T_Embed_PN532.h): 170×320 panel, MOSI9, MISO10, SCK11, CS41, DC16, backlight21, reset=-1, inversion enabled.
- [Pinned initialization](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/c36826e0f91311cb591c4545fccf91e254fc55dc/lib/TFT_eSPI/TFT_Drivers/ST7789_Init.h) and [rotation](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/c36826e0f91311cb591c4545fccf91e254fc55dc/lib/TFT_eSPI/TFT_Drivers/ST7789_Rotation.h): RGB565, landscape MX|MV and row offset35, normal mode/inversion/power/gamma setup. The implementation uses software reset, then releases the bus during reset/sleep/display-on delays. It does not drive GPIO40 as a reset.

The manufacturer's TFT_eSPI HSPI recommendation addresses that library's direct register implementation. This module instead uses Arduino's shared `SPI` object with explicit transactions and the existing radio pins. It does not instantiate TFT_eSPI, remap pins, call `SPI.end()`, change SPI pin modes, or mask interrupts.

Each LCD transfer tries the existing recursive radio mutex with zero wait. A busy radio skips the transfer. CS41 is raised before transaction end; other slave selects stay deselected. The radio driver selects 4 MHz/mode0/MSB-first on every access so it cannot inherit the LCD's 20 MHz setting. The selected 4 MHz clock is below the limits in [TI’s SPI timing specification](https://www.ti.com/lit/ds/symlink/cc1101.pdf), including burst transfers without inserted delays. Reset ends its transaction before register helpers open theirs. Existing radio preflight may still end/reinitialize the bus under that same mutex; the display reopens it with the exact existing pins on its next transfer. GPIO16/DC and GPIO0/wake are reserved against saved/imported relay assignments in the shared CC1101/Plus profile.

Snapshots are sampled once per second after setup. Only changed text rows transfer, at most one row per 20 ms. A row transfers 11,520 pixel bytes plus 11 command/address bytes; initialization clears two scanlines per pass. At 20 MHz the pixel wire time per text row is 4.608 ms, before CPU/driver overhead. This is a calculated bound on wire volume, not a hardware latency measurement. The first BOOT page has a bounded 1.2-second setup window; initialization has a five-second elapsed-time failure limit and never waits for the radio lock.

The renderer uses a 720-byte monochrome row canvas and a 640-byte static pixel scanline, not a 108,800-byte full-screen framebuffer. It requests no PSRAM or DMA buffers. Allocation/PWM/SPI initialization/initialization timeout failures leave the backlight off and let controller startup continue. The LCD is write-only: command completion cannot detect a disconnected or damaged panel; a dark panel must not be interpreted as controller failure.

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

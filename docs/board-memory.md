# LILYGO memory use

Both canonical physical-board builds use ESP32-S3 N16R8, QIO/80 MHz flash and OPI PSRAM. The application table remains `default_8MB.csv` with two 3,342,336-byte app slots and 1,572,864-byte LittleFS; 16 MB physical flash does not expand these slots automatically.

| Allocation | Requested memory | Bytes | Lifetime |
| --- | --- | ---: | --- |
| RGB565 framebuffer | Explicit PSRAM | 108,800 | UI lifetime |
| Optional immutable BMP | Explicit PSRAM | 108,866 | At most one HTTP response |
| Four-row transfer band | Internal static RAM | 2,560 | UI lifetime |
| Fallback monochrome text canvas | Small heap | 720 | Display lifetime |
| Fallback pixel scanline | Internal static RAM | 640 | Display lifetime |

Framebuffer allocation never falls back to internal SRAM. Failure uses the simple status screen; additional PWM/SPI/text-buffer failure leaves the backlight off and lets controller servicing continue. Standard-board runtime qualification remains distinct from Plus qualification.

ELF static-RAM and firmware-size reports are compile evidence. They do not measure internal minimum/largest free blocks, TLS/staging coexistence, fragmentation, I2C/SPI duration or real loop latency. Keep these runtime measurements pending until an exact live stage is authorized; see the [build audit](lcd-build-audit.md) and [UI memory details](lilygo-local-ui.md#memory-and-rendering).

Build checks for the canonical configuration and caller-owned parser-error prerequisite are recorded in the [issue 25 build audit](lcd-build-audit.md#canonical-build-implementation--offline-evidence). Runtime measurements remain pending.

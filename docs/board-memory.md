# RAM in the supported board configurations

The LILYGO T-Embed CC1101 Plus has 8 MiB octal PSRAM in addition to its ESP32-S3's internal SRAM. Several other supported configurations have no PSRAM enabled, so the GitHub updater is enabled only for the exact Plus profile.

This table covers every firmware environment in this branch's `platformio.ini`, checked against the pinned pioarduino `55.03.312` board definitions on November 6, 2026. The native/coverage environments run on a host computer and have no board memory contract. Sizes below describe physical SRAM and configured external memory, **not available heap**; code, stacks, caches, drivers and reserved regions consume part of these totals.

| Firmware environment | Configured board | Internal SRAM | PSRAM selected by this configuration | Flash selected |
| --- | --- | --- | --- | --- |
| `esp32dev` | Generic ESP32 Dev Module | 520 KiB main + 16 KiB RTC | None enabled; physical module varies | 4 MiB |
| `esp32s3` | ESP32-S3-DevKitC-1-N8 | 512 KiB main + 16 KiB RTC | None; board definition explicitly says “No PSRAM” | 8 MiB |
| `esp32s3dbg` | ESP32-S3-DevKitC-1-N16R8V | 512 KiB main + 16 KiB RTC | 8 MiB octal, QIO flash / OPI RAM SDK | 16 MiB |
| `lilygo-t-embed-cc1101-plus` | Exact T-Embed CC1101 Plus profile, N16R8 | 512 KiB main + 16 KiB RTC | 8 MiB octal, QIO flash / OPI RAM SDK | 16 MiB; partition table uses lower 8 MiB |
| `esp32c3` | ESP32-C3-DevKitM-1 | 400 KiB main (includes 16 KiB cache) + 8 KiB RTC | None enabled | 4 MiB |
| `esp32c5` | ESP32-C5-DevKitC-1, pinned older 4 MiB profile | 384 KiB HP + 16 KiB LP | None enabled by the pinned board definition | 4 MiB |
| `esp32c6` | Seeed XIAO ESP32C6 | 512 KiB HP + 16 KiB LP | None; C6 does not support external PSRAM | 4 MiB |
| `esp32p4` | Generic P4 revision 3.01 or later | 768 KiB HP + 32 KiB LP + 8 KiB TCM | Enabled (`BOARD_HAS_PSRAM`, 200 MHz); physical capacity not declared by generic board profile | 16 MiB |
| `esp32p4es` | Generic early P4 revision before 2.0 | 768 KiB HP + 32 KiB LP + 8 KiB TCM | Enabled (`BOARD_HAS_PSRAM`, 200 MHz); physical capacity not declared by generic board profile | 4 MiB |

An ESP32-S3 chip name does not imply PSRAM is present. An N8 board, N16R8 board and Plus board are different compatibility targets even when their CPU is the same. The generic P4 profiles do not establish which physical module is attached; an actual board's PSRAM capacity must be identified independently before promising an updater memory budget.

Similarly, current ESP32-C5-DevKitC-1 v1.2 boards are sold as **N8R8 with 8 MiB PSRAM**. That newer hardware does not change the pinned project's older C5 configuration, which assumes 4 MiB flash and enables no PSRAM. Generic ESP32 hardware can also use a PSRAM-equipped WROVER module, but this project's generic `esp32dev` profile does not enable it.

The historical original-firmware branch also built ESP32-S2. It is not an environment in the maintained cjkas-based branch. ESP32-S2 has 320 KiB main SRAM plus 16 KiB RTC SRAM; PSRAM depends on the module (including variants with 2 MiB), rather than universally providing 8 MiB.

## Sources

- [Pinned project build environments](../platformio.ini) and [pioarduino's pinned board definitions](https://github.com/pioarduino/platform-espressif32/tree/55.03.312/boards) determine what the firmware actually enables. The commonly repeated `maximum_ram_size: 327680` board metadata is a build reporting limit, not the chip's total physical SRAM or PSRAM capacity.
- Espressif datasheets: [ESP32](https://documentation.espressif.com/esp32_datasheet_en.html), [ESP32-S3](https://www.espressif.com/en/products/socs/esp32-s3), [ESP32-C3](https://documentation.espressif.com/esp32-c3_datasheet_en.html), [ESP32-C5](https://documentation.espressif.com/esp32-c5_datasheet_en.html), and [ESP32-S2](https://documentation.espressif.com/esp32-s2_datasheet_en.html).
- [Espressif's memory map overview](https://developer.espressif.com/blog/2024/08/esp32-memory-map-101/) distinguishes HP, LP and TCM SRAM across the families; [Seeed's XIAO C6 documentation](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/) identifies its 512 KiB SRAM and 4 MiB flash. [Espressif's PSRAM FAQ](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/storage/psram.html) confirms the C6 limitation.
- [LILYGO's exact Plus product](https://lilygo.cc/products/t-embed-cc1101-plus) specifies ESP32-S3, 16 MiB flash and 8 MiB PSRAM. [Espressif's current C5 DevKitC-1 guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c5/esp32-c5-devkitc-1/user_guide.html) documents the newer N8R8 variant.

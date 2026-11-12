# RAM in the supported board configurations

Verified GitHub updates require a selected exact-board release profile with at least 8 MiB working PSRAM, matching flash/partitions, enough application-slot headroom, matching packaged web assets, and an attested rollback-enabled bootloader. PSRAM capacity alone does not enable installation. Several supported configurations have no PSRAM enabled; they retain manual updates.

This table covers every firmware environment in this branch's `platformio.ini`, checked against the pinned pioarduino `55.03.312` board definitions on November 6, 2026. The additional PSRAM release targets are explicit opt-ins; existing no-PSRAM and single-slot debug targets retain their layouts. The native/coverage environments run on a host computer and have no board memory contract. Sizes below describe physical SRAM and configured external memory, **not available heap**; code, stacks, caches, drivers and reserved regions consume part of these totals.

| Firmware environment | Configured board | Internal SRAM | PSRAM selected by this configuration | Flash selected |
| --- | --- | --- | --- | --- |
| `esp32dev` | Generic ESP32 Dev Module | 520 KiB main + 16 KiB RTC | None enabled; physical module varies | 4 MiB |
| `esp32s3` | ESP32-S3-DevKitC-1-N8 | 512 KiB main + 16 KiB RTC | None; board definition explicitly says “No PSRAM” | 8 MiB |
| `esp32s3dbg` | ESP32-S3-DevKitC-1-N16R8V | 512 KiB main + 16 KiB RTC | 8 MiB octal, QIO flash / OPI RAM SDK; single-slot debug, no OTA | 16 MiB |
| `esp32s3-psram` | ESP32-S3-DevKitC-1-N16R8V release profile | 512 KiB main + 16 KiB RTC | 8 MiB octal; dual-slot updater profile | 16 MiB; table uses lower 8 MiB |
| `lilygo-t-embed-cc1101-plus` | Full-feature T-Embed CC1101 Plus, N16R8 | 512 KiB main + 16 KiB RTC | 8 MiB octal; explicit 108,800-byte UI framebuffer | 16 MiB; partition table uses lower 8 MiB |
| `lilygo-t-embed-cc1101` | Standard T-Embed CC1101, N16R8 | 512 KiB main + 16 KiB RTC | 8 MiB octal; same UI framebuffer, separate identity from Plus | 16 MiB; table uses lower 8 MiB |
| `esp32c3` | ESP32-C3-DevKitM-1 | 400 KiB main (includes 16 KiB cache) + 8 KiB RTC | None enabled | 4 MiB |
| `esp32c5` | ESP32-C5-DevKitC-1, pinned older 4 MiB profile | 384 KiB HP + 16 KiB LP | None enabled by the pinned board definition | 4 MiB |
| `esp32c5-psram` | Official current C5 DevKitC-1 N8R8 release profile | 384 KiB HP + 16 KiB LP | 8 MiB quad, enabled explicitly; dual-slot updater profile | 8 MiB |
| `esp32c6` | Seeed XIAO ESP32C6 | 512 KiB HP + 16 KiB LP | None; C6 does not support external PSRAM | 4 MiB |
| `esp32p4` | Generic P4 revision 3.01 or later | 768 KiB HP + 32 KiB LP + 8 KiB TCM | Enabled (`BOARD_HAS_PSRAM`, 200 MHz); physical capacity not declared by generic board profile | 16 MiB |
| `esp32p4es` | Generic early P4 revision before 2.0 | 768 KiB HP + 32 KiB LP + 8 KiB TCM | Enabled (`BOARD_HAS_PSRAM`, 200 MHz); physical capacity not declared by generic board profile | 4 MiB |
| `esp32p4-psram` | Explicit P4 release updater layout, revision 3.01 or later | 768 KiB HP + 32 KiB LP + 8 KiB TCM | Enabled, 200 MHz; runtime requires at least 8 MiB | 16 MiB; `default_8MB` table uses lower 8 MiB |
| `esp32p4es-psram` | Explicit early-P4 release updater layout | 768 KiB HP + 32 KiB LP + 8 KiB TCM | Enabled, 200 MHz; runtime requires at least 8 MiB | 4 MiB; separate `esp32_psram_4MB` table |

An ESP32-S3 chip name does not imply PSRAM is present. An N8 board, N16R8 board and Plus board are different compatibility targets even when their CPU is the same. The generic P4 profiles do not establish which physical module is attached; an actual board's PSRAM capacity must be identified independently before promising an updater memory budget.

Similarly, current ESP32-C5-DevKitC-1 v1.2 boards are sold as **N8R8 with 8 MiB PSRAM**. Use the new `esp32c5-psram` profile for that N8R8 hardware. The older `esp32c5` configuration still assumes 4 MiB flash, enables no PSRAM and uses a single application slot. Generic ESP32 hardware can also use a PSRAM-equipped WROVER module, but this project's generic `esp32dev` profile does not enable it.

The update profiles are centralized in [`update-profiles.json`](../update-profiles.json): full-feature Plus and standard LILYGO, PSRAM S3 release, PSRAM C5 release, `esp32p4-psram` and `esp32p4es-psram`. Both canonical LILYGO builds include the display, local UI and local shutter commands, with separate board identities. Headless/display-only/UI-preview build variants are retired; dated reports retain their original environment names and hashes. S3/C5 and the new 16 MiB P4 release profile use `default_8MB` dual slots of `0x330000` bytes. The new 4 MiB early-P4 release profile uses two `0x1C0000`-byte slots and a 384 KiB shared filesystem. Existing `esp32p4`/`esp32p4es` retain `esp32_3MB` slots of `0x180000` bytes and manual updates: the observed TLS-enabled P4 image was 1,646,170 bytes and did not fit those 1,572,864-byte slots. Moving to an explicit new layout requires approved provisioning and fresh state preservation; firmware-only OTA never migrates it. Unconfigured flash/PSRAM variants require a reviewed exact profile. Standard LILYGO and the added memory variants have not been tested on hardware by this task.

Both canonical LILYGO builds allocate their UI framebuffer from PSRAM without an internal-SRAM fallback. One optional screenshot adds 108,866 bytes of PSRAM while its HTTP response is in flight. [The UI memory table](lilygo-local-ui.md#memory-and-rendering) describes the internal transfer/fallback buffers. These allocations reduce free capacity available to a background update; complete staging and internal-memory checks must still succeed. Prior display/UI validation does not establish combined updater responsiveness or authorize this task to access the controller.

The historical original-firmware branch also built ESP32-S2. It is not an environment in the maintained cjkas-based branch. ESP32-S2 has 320 KiB main SRAM plus 16 KiB RTC SRAM; PSRAM depends on the module (including variants with 2 MiB), rather than universally providing 8 MiB.

## Sources

- [Pinned project build environments](../platformio.ini) and [pioarduino's pinned board definitions](https://github.com/pioarduino/platform-espressif32/tree/55.03.312/boards) determine what the firmware actually enables. The commonly repeated `maximum_ram_size: 327680` board metadata is a build reporting limit, not the chip's total physical SRAM or PSRAM capacity.
- Espressif datasheets: [ESP32](https://documentation.espressif.com/esp32_datasheet_en.html), [ESP32-S3](https://www.espressif.com/en/products/socs/esp32-s3), [ESP32-C3](https://documentation.espressif.com/esp32-c3_datasheet_en.html), [ESP32-C5](https://documentation.espressif.com/esp32-c5_datasheet_en.html), and [ESP32-S2](https://documentation.espressif.com/esp32-s2_datasheet_en.html).
- [Espressif's memory map overview](https://developer.espressif.com/blog/2024/08/esp32-memory-map-101/) distinguishes HP, LP and TCM SRAM across the families; [Seeed's XIAO C6 documentation](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/) identifies its 512 KiB SRAM and 4 MiB flash. [Espressif's PSRAM FAQ](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/storage/psram.html) confirms the C6 limitation.
- [LILYGO's exact Plus product](https://lilygo.cc/products/t-embed-cc1101-plus) and [standard CC1101 product](https://lilygo.cc/products/t-embed-cc1101) specify ESP32-S3, 16 MiB flash and 8 MiB PSRAM. [Espressif's current C5 DevKitC-1 guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c5/esp32-c5-devkitc-1/user_guide.html) documents the newer N8R8 variant.

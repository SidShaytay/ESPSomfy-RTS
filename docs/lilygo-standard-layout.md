# Standard flash layout experiment

This offline Plus build uses Arduino’s `default_8MB.csv` on the existing 16 MB flash / 8 MB OPI PSRAM hardware configuration. It does not select a final firmware fork or authorize installation. The worktree starts from `9f61a7c`, including the integrated security fixes and Arduino 3 radio-initialization fix.

## Why use this layout

- cjkas’s generic S3 release inherits this exact table from its pinned pioarduino board definition. Its 16 MB debug target uses a separate single-app table and is unsuitable for retaining firmware OTA.
- xkain’s explicit S3 builds currently select the smaller `default.csv`; it has no explicit 16 MB or T-Embed Plus build. Matching that table would limit each firmware slot to 1.25 MiB. This candidate’s firmware is already 1,332,528 bytes, exceeding such a slot by 21,808 bytes. This compares the layout against our cjkas-based binary, not against xkain’s own firmware size.
- The standard 8 MB table gives two 3,342,336-byte OTA slots and a 1,572,864-byte filesystem. A custom or standard 16 MB table would move partitions again without a current capacity need.
- Flash above 8 MB remains unallocated. A future xkain Plus port could select the same standard table, but still needs its own hardware port, build and compatibility validation. Matching addresses does not make firmware or backup formats interchangeable.

Sources checked on 2026-11-06: [cjkas configuration](https://github.com/cjkas/ESPSomfy-RTS/blob/main/platformio.ini), [S3 board definition](https://github.com/pioarduino/platform-espressif32/blob/55.03.312/boards/esp32-s3-devkitc-1.json), [standard table](https://github.com/espressif/arduino-esp32/blob/3.3.12/tools/partitions/default_8MB.csv), and [xkain configuration](https://github.com/xkain/ESPSomfy-RTS/blob/main/platformio.ini).

## Changes and evidence

The Plus environment selects `default_8MB.csv` directly; the custom CSV is removed. Package validation checks the standard offsets and sizes while retaining 16 MB image headers and QIO/OPI/USB settings. Tests use an independent standard-layout fixture and reject the previous custom table with a valid MD5.

- Exact Plus release build and LittleFS build passed. Firmware: 1,332,528 bytes, leaving 2,009,808 bytes in each OTA slot.
- The compiled `partitions.bin` matches the pinned SDK’s `default_8MB.csv` generated binary byte for byte.
- Manifest/package tests: 24 passed. The standard-layout fixture was rejected before the implementation update; an in-memory mutation of the second OTA address is detected.
- Board/SPI/reserved-GPIO harness and non-S3 rejection passed.
- Native tests: 638 passed, one expected skip for the optional private backup fixture. Existing container-backed compiler wrappers were used.
- JavaScript tests: 62 passed. Release-version script passed.

Artifacts are local under `build/lilygo/artifacts/`: firmware, filesystem, bootloader, partition table, OTA initialization image, symbols, compiler metadata and `flash-manifest.json`. The manifest records the hashes and exact layout. Build logs are in `build/lilygo/`. One inherited compiler warning remains: deprecated increment of a volatile-qualified value in `src/Transceiver.cpp:188`.

The installed controller and all existing worktrees remain unchanged. Neither the installed table nor the previous custom candidate table can be migrated by firmware OTA alone. See [LILYGO.md](../LILYGO.md) for the layout and migration limits. No serial access, flashing, reboot, RF or motor operation was performed. The earlier [port validation report](lilygo-port-validation.md) records the previous custom-layout build, not these artifacts.

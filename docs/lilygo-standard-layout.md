# Standard flash layout experiment

This Plus build uses Arduino’s `default_8MB.csv` on the existing 16 MB flash / 8 MB OPI PSRAM hardware configuration. It does not select a final firmware fork; the later authorized installation is recorded below. The worktree starts from `9f61a7c`, including the integrated security fixes and Arduino 3 radio-initialization fix.

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

During the offline build described above, the controller and other worktrees were unchanged. Neither the installed table nor the previous custom candidate table can be migrated by firmware OTA alone. See [LILYGO.md](../LILYGO.md) for the layout and migration limits. No device operations were performed during that offline build. The earlier [port validation report](lilygo-port-validation.md) records the previous custom-layout build, not these artifacts.

## Installed-layout verification

On November 6, 2026, Sid authorized installation with a fresh counter backup and a state-preserving migration. Installed build: `6842f0a22a334c849ab1c5ed11e6c474c6666aed`. The previous running build was the corrected cjkas Plus revision `9f61a7c`, using the earlier custom layout; it was no longer v2.4.7.

- Captured the complete current 16 MB image, portable export and protocol/settings snapshots. The board stayed in ROM through snapshot preparation, writing and full preboot verification. Automations were not paused; the ROM hold prevented controller-side commands during that interval.
- Verified identity, flash size, unsecured recovery, partition-table MD5, active NVS CRCs and independent authoritative configuration extraction. The installed firmware matched the retained corrected build.
- Preserved `shades.cfg` byte for byte in the new filesystem, along with all eight pristine web files. The parser round-trip compared 68 configuration fields. Independently reconstructed the sector plan, excluding NVS at `0x9000–0xdfff`.
- Wrote only changed segments: partition table at `0x8000`, firmware at `0x10000`, and state-preserving LittleFS at `0x670000`. Bootloader and OTA-selection regions already matched and were skipped. No whole-chip erase was used.
- The complete 16 MB preboot readback matched the expected image byte for byte. NVS was untouched by writes.
- Both application boots passed radio initialization/register/RX checks and 52 read-only protocol comparisons. All five settings sections and 48 entity-field comparisons matched before/after. Postboot extraction found all nine application NVS namespaces unchanged and authoritative `shades.cfg` byte-identical. Counters did not change during verification.
- A 3,000,000-baud read test matched expected bytes, and subsequent NVS/filesystem captures at that setting passed. Measured filesystem read throughput was about 817 kbit/s; the baud setting is not the actual end-to-end transfer rate.

Private recovery images, decoded settings, the exact execution plan and 154 checksummed evidence files remain under the user-local `espsomfy-lilygo` state directory. No raw state or credentials are committed. `LATEST_LAYOUT_MIGRATION.json` points to the completed record. The build artifacts identify revision `6842f0a`; this later documentation update does not change them.

The controller is online with the standard layout. No assistant RF/motor commands, pairing, retuning or motor-limit changes were performed. Physical movement and cold power-cycle behavior were not tested in this cycle. Existing automations can advance counters after verification; capture current state again before any future restore. The earlier custom-layout recovery snapshot is a rollback record, not a perpetually current counter source.

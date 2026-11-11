# Install from another firmware and keep your shutters

Use this guide to move an existing **LILYGO T-Embed CC1101 or CC1101 Plus** to this fork. Preserve the controller’s radio identities and rolling codes so you do not have to pair the motors again. A firmware version alone does not tell you which flash layout or backup format is installed.

## 1. Save the current controller before installing

In your existing firmware, download a fresh **Backup** export. Also record radio settings, device/group names and addresses, group membership, travel/tilt calibration, rooms, network/MQTT settings and security mode. Keep these files private: backups and raw flash can contain credentials.

Take a full-flash recovery copy using the [backup procedure](flashing.md#2-capture-a-fresh-recovery-set), including its ROM chip/capacity/security checks. On the supported N16R8 boards that is a **16,777,216-byte** image. Verify the partition-table checksum and independently extract the authoritative configuration and NVS counters; a file hash alone does not prove a usable recovery set.

Arrange a quiet interval without automation or shutter commands while capturing and migrating. Rolling codes can advance after a backup; an old export or dump is not a permanently safe restore point. Never run two controllers with the same radio identities.

## 2. Choose the path from the actual partition table

Compare your board’s decoded table with the [target layout](../LILYGO.md#partitions-and-migration-limits), not with a firmware name:

| Existing controller | First installation path |
| --- | --- |
| Matching `default_8MB` layout, two valid OTA slots, compatible ESPSomfy configuration | Firmware-only upload through the existing browser; follow step 3 below. Verify bootloader/recovery compatibility too. |
| Original v2.x, custom LILYGO table, single-slot debug build, smaller app slots, or different filesystem address | Board-specific USB migration; follow step 4. Firmware OTA cannot change the partition table. |
| Unrelated firmware, unknown backup format or unreadable current state | Extract and verify the settings/counters first. This project cannot promise to import arbitrary firmware formats. Do not overwrite the only recoverable copy. |

The target NVS is at `0x9000` (size `0x5000`), app slots at `0x10000` and `0x340000` (each `0x330000`), and LittleFS at `0x670000` (size `0x180000`). The historical v2.4.7 filesystem starts at `0x290000`, **inside a new application slot**. Keeping NVS at its old address is therefore insufficient to preserve shutter configuration.

## 3. Install through the existing browser on a matching layout

Download **`firmware-lilygo-t-embed-cc1101.bin`** from the chosen release. Use the shared image for both supported boards. Check its size and SHA256 against the release’s `update-lilygo-t-embed-cc1101.json` and the actual partition capacity. These checks identify the intended file; they do not replace checking the release source and board compatibility.

In the existing browser interface, open its firmware page and select **Update Firmware**. Upload only the firmware binary. Some older ESPSomfy web pickers require `.ino.` in the filename; if yours does, rename a copy to `SomfyController.ino.lilygo-t-embed-cc1101.bin` and verify that its SHA256 is unchanged. Do not select Restore, a web/application update, a filesystem image or a merged factory image.

Keep power connected. After reboot, open **System → About**, verify the new version and boot health, and compare fresh radio/settings/device/group/calibration/counter records with the saved state. Follow [post-install verification](flashing.md#5-verify-the-reboot-without-moving-anything) before restoring automation. This path leaves NVS, LittleFS and the old active app intact; it needs no configuration restore when state loads correctly.

Engineering 4.9.x controllers need this manual path to install public v4.1.0: the GitHub updater correctly refuses a lower version. Once v4.1.0 is installed, newer eligible public releases can use **Home → Updates**.

## 4. Migrate a different layout over USB

There is no safe universal flash command for an already configured board. Prepare the migration from the **fresh raw backup**, before writing:

1. Decode the old partitions and active app; extract the authoritative `shades.cfg`, all NVS settings/counters and any other private files. Confirm the extracted model matches the portable export and live state.
2. Build a target filesystem containing that configuration, with the target firmware’s compatible filesystem tools. Preserve radio addresses, calibration, group membership and current primary **and linked-remote** counters. Do not substitute an empty public filesystem.
3. Specify every write range and preserved range. Preserve compatible NVS in place, account for sector erasure and any bootloader/OTA-selection changes, and make sure the new app fits its slot. A merged image can overwrite NVS with padding even when the nominal addresses match.
4. Keep the board in ROM while writing the reviewed segments and reading them back. Compare the complete readback with the expected image before booting. Then boot once and perform the same state and health checks as step 3.

The [standard-layout migration record](lilygo-standard-layout.md#installed-layout-verification) documents a tested same-board migration that preserved NVS and rebuilt LittleFS with the original configuration. Its three-segment write plan applied to that board’s verified bootloader and OTA state; copying those addresses without your own comparison is not a migration procedure.

The format24 importer has offline compatibility coverage and keeps the higher existing primary counter on the same controller. A format24 export contains linked-remote addresses **but not their counters**. On an empty board, an export alone cannot reconstruct those missing counters. Retain the original NVS/full-flash data; do not erase, regenerate identities, re-pair or recalibrate to work around missing state.

A blank new board has no shutter settings to preserve and needs a separately verified provisioning plan. The published OTA image alone does not provision its bootloader, table, OTA metadata and writable filesystem.

If any comparison fails, stop and preserve both the original and current flash. Use the [recovery procedure](flashing.md#if-installation-or-verification-fails) rather than restoring a stale dump over newer counters.

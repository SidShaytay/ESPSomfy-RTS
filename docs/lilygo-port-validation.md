# Plus forward-port validation

Offline evaluation for [issue #13](https://github.com/SidShaytay/ESPSomfy-RTS/issues/13), 2026-11-04. Source baseline: cjkas v4.0.2, `07bffdec6b540cc99f31d02d7e6de6d3d1e7f8f3`. The upstream import is a separate merge commit, followed by the hardware/state/build adaptation. The fork remains `SidShaytay/ESPSomfy-RTS`.

## Automated checks

| Check | Result and scope |
|---|---|
| Native C++ | 633 cases: 632 passed, one expected skip for optional private `CONFIG_BACKUP`. New synthetic format24 parser/import-policy tests included. |
| Web protocol | `node --test 'test/js/*.test.js'`: 60 passed. No web interface behavior changed by the Plus profile. |
| Manifest/build configuration/package handling | `python3 tools/test_lilygo_manifest.py`: 23 passed, including incorrect board/offset/size/header/hash, conflicting or undefined macros, wrong memory settings, source provenance, and failed-build output preservation. |
| Release version script | `bash test/scripts/test_set_version.sh`: passed. |
| Board/SPI/GPIO harness | Plus and generic behavior passed; non-S3 profile rejected. Production `ShadeGpio.cpp` tested with every reserved board/USB/flash pin in each output slot; no writes, while allowed pins still work. |
| HA protocol compatibility | Pinned cjkas HA v3.0.4, `0d3bc1349ac1047c843e87ea21381b069d786fb2`: 51 protocol/fake-device tests passed. `hello.json`, `schema.json`, `sync.json` fixtures byte-for-byte identical to the firmware fixtures. |

The new SPI ownership assertion failed before the Arduino 3 preflight fix. Removing the runtime reserved-GPIO guard in a scratch copy made the production-method harness fail its no-write assertion. A deliberate rolling-code `max` to `min` mutation failed the helper and synthetic import tests (three failures); restored tests passed. These checks establish that the tests detect the regressions they target.

Fedora Silverblue had no host C++ compiler. Native and GPIO harnesses used the existing `rocm-7.2.3` container’s GCC through user-cache wrappers; no system packages were installed. The normal commands in `LILYGO.md` work in an environment with GCC/G++ on PATH. Web tests used the installed Node runtime. HA tests ran from a pinned temporary checkout with Python 3.13 and its exact `requirements_test.txt`, using `pytest tests/proto -q`; this was not a Home Assistant installation change.

## Build artifacts

Final release-mode builds passed for Plus, generic ESP32, S3 and C3. Their actual firmware binaries passed the partition/headroom checks:

| Target | Firmware bytes | App slot bytes |
|---|---:|---:|
| LILYGO Plus | 1,331,024 | 3,145,728 |
| Generic ESP32 | 1,410,320 | 1,572,864 |
| Generic S3 | 1,330,912 | 3,342,336 |
| Generic C3 | 1,503,056 | 1,572,864 |

The final Plus package also contains a 19,968-byte bootloader, 3,072-byte partition table, 8,192-byte OTA initialization image, and 1,441,792-byte LittleFS image. The firmware has 1,814,704 bytes of app-slot headroom. Artifact SHA256 values are in the generated manifest; the compiled source input hashes were checked unchanged after packaging.

Run `bash tools/build-lilygo.sh` to produce the firmware, ELF symbols, bootloader, partition table, OTA initialization image, LittleFS and `flash-manifest.json` under `build/lilygo/artifacts/`. Use:

```bash
python3 tools/test_lilygo_manifest.py
python3 tools/lilygo_manifest.py validate build/lilygo/artifacts
```

The manifest validates the actual partition table and MD5, ESP32-S3 image headers/checksums/appended SHA256, exact offsets, region sizes, firmware headroom and per-file SHA256. Source input hashes and revision identify the build; resolved pioarduino configuration and actual compiler defines/flags/SDK includes check QIO/OPI/USB/profile settings. Package generation uses a fresh staging directory, validates it before publication, and preserves previous output separately. ROM headers use DIO before the SDK enables QIO. Board identity/configuration validation is not device detection, a trusted signature, or installation authorization.

An initial generic C3 compile lost its build directory during an overlapping `pio test` invocation. PlatformIO’s automatic project-checksum cleanup deletes the shared `.pio/build` directory; final firmware builds were rerun with exclusive access. Do not run native tests and firmware builds concurrently in one checkout.

The dedicated `.github/workflows/lilygo.yaml` retains reviewable Plus assets and runs the board/manifest checks. The inherited generic release publisher is separate; this offline candidate has not been published as a release. Device-side GitHub pull updates remain work for #12. The pinned v4.0.2 source tag contains `data-src/appversion` = `4.0.1`, inherited by the compiler’s `FW_VERSION`; this discrepancy is retained explicitly, not presented as a final fork release identity. #12 owns that release/version policy.

## Review and limits

Independent review checked radio lock order, nonblocking atomic readiness publication, queued-job readiness rechecks, reserved GPIO import/runtime protection, Arduino 3 SPI ownership, counter preservation, image validation and packaging. Review fixes are covered by the focused tests. Failed radio reconfiguration blocks software use, but does not guarantee the physical chip has left a prior RX state.

Synthetic import tests call the real `ConfigRecords` parser and production rolling-code policy with simulated per-address NVS. They do not execute the ESP32 filesystem/NVS `ShadeConfigFile` restoration path end to end. No private backup was read for this port. Format24 cannot supply linked-remote counters to an empty replacement NVS; the inherited 16-bit next-code wrap behavior remains unchanged.

No live controller connection, flash, erase, reboot, pairing, calibration, RF transmission or motor command was performed. Actual hardware boot/RF/HA import, rollback and faithful state restoration remain unverified. The larger app layout overlaps the installed filesystem, so deployment requires the separate recovery and migration review described in `LILYGO.md` and issue #9. Preserve current counters and do not run duplicate radio identities.

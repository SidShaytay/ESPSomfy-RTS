# Repository Guidelines

## Project Structure & Module Organization

`SomfyController.ino` contains Arduino startup and the main loop. Root-level `.cpp`/`.h` pairs implement radio and shade control (`Somfy`), networking (`Network`, `MQTT`, `Sockets`, `SSDP`), HTTP handling (`Web`, `WResp`), configuration, and OTA updates (`GitOTA`).

`data/` contains the HTML, JavaScript, CSS, images, and application version packaged into LittleFS. `icons/` holds additional artwork. `.github/workflows/` defines CI and release builds. Root-level `.bin` files are firmware/filesystem artifacts. There is no dedicated test directory.

## Build, Test, and Development Commands

Install Arduino CLI and the ESP32 core, ArduinoJson, PubSubClient, SmartRC-CC1101-Driver-Lib, and WebSockets. Match dependency versions and board settings to the relevant workflow; CI and release configurations differ.

Arduino requires the sketch directory to match `SomfyController.ino`. From the repository root, create a temporary sketch and compile for the basic ESP32 CI target:

```sh
sketch_root=$(mktemp -d)
mkdir "$sketch_root/SomfyController"
cp ./*.ino ./*.cpp ./*.h "$sketch_root/SomfyController/"
arduino-cli compile --clean --fqbn esp32:esp32:esp32 --warnings default "$sketch_root/SomfyController"
```

With `mklittlefs` built as described in the workflows, package web assets:

```sh
./mklittlefs/mklittlefs --create data --size 1441792 SomfyController.littlefs.bin
```

Run firmware on an ESP32 with the matching filesystem image. Serial logging uses 115200 baud; access the web UI through the device address.

## Coding Style & Naming Conventions

Follow nearby code: C++ generally uses two-space indentation and opening braces on the same line; JavaScript and CSS generally use four spaces. Preserve local exceptions. Use PascalCase classes/module filenames, camelCase methods and variables, and uppercase macro constants. No formatter or linter is configured; avoid unrelated reformatting.

## Testing Guidelines

No automated test framework, test naming convention, or coverage threshold is configured. CI compiles ESP32, C3, S2, and S3 targets and builds LittleFS. For affected features, verify physical shade commands, remote reception, network reconnection, MQTT/socket events, and web interactions. Record hardware, firmware/application versions, reproduction steps, and relevant serial logs.

## Commit & Pull Request Guidelines

History uses short, action-oriented subjects, often with issue references: `Fix issue with deleting repeaters #402`. Keep commits focused. In pull requests, explain the problem and resulting behavior, link relevant issues, and report builds and hardware checks. Include screenshots for visible UI changes. Keep generated binaries out of routine source changes unless preparing release artifacts.

## Issue Workflow

Track ordered work in [GitHub Issues](https://github.com/SidShaytay/ESPSomfy-RTS/issues). Use a parent issue with sub-issues, explicit dependencies, acceptance criteria, and verification steps. Start ready tasks in order; record evidence and blockers before closing them. Keep credentials and flash backups local. For the LILYGO port, start at [issue #1](https://github.com/SidShaytay/ESPSomfy-RTS/issues/1) and `tasks/plan.md`. Use separate branches/worktrees for concurrent edits.

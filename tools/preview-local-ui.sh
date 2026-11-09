#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
gfx="${GFX_DIR:-$repo/.pio/libdeps/lilygo-t-embed-cc1101/Adafruit GFX Library}"
out="${1:-$repo/build/ui-preview}"
json="${ARDUINOJSON_DIR:-$repo/.pio/libdeps/native/ArduinoJson/src}"
[[ -f "$gfx/Adafruit_GFX.cpp" ]] || { echo 'Build the UI env first, or set GFX_DIR to its pinned Adafruit GFX library directory.' >&2; exit 1; }
mkdir -p "$out"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
"${CXX:-c++}" -std=c++17 -O2 -DARDUINO=10800 -DLOCAL_UI_HOST_PREVIEW \
  -I"$repo/tests/ui-render" -I"$repo/tests/ui-adapter" -I"$repo/test/stubs" -I"$repo/src" -I"$gfx" \
  "$repo/tests/ui-render/render.cpp" "$repo/src/LocalUiRenderer.cpp" "$repo/src/StatusDisplay.cpp" "$gfx/Adafruit_GFX.cpp" \
  -o "$scratch/render"
"$scratch/render" "$out"
# The actual ownership service is compiled without SDK/worker startup paths.
# Runtime still calls its real locked configuration lease implementation.
"${CXX:-c++}" -pthread -std=c++17 -O2 -I"$json" -I"$repo/src" -I"$repo/tests/update-certificate" \
  -c "$repo/src/GitUpdate.cpp" -o "$scratch/configuration-owner.o"
# 64 host slots request the target's 1 KiB JSON pool (host slots are larger).
# This is a conservative allocation-path check, not a physical heap benchmark.
"${CXX:-c++}" -std=c++17 -O2 -DARDUINO=10800 -DESPSOMFY_LOCAL_UI -DESPSOMFY_LOCAL_COMMANDS -DUI_RUNTIME_HOST_TEST -DLOCAL_QUEUE_REAL_UPDATE_OWNER \
  -DESPSOMFY_LILYGO_T_EMBED_CC1101 -DCONFIG_IDF_TARGET_ESP32S3 \
  -DLOCAL_SETTINGS_HOST_TEST -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 \
  -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_PROGMEM=0 -DARDUINOJSON_POOL_CAPACITY=64 \
  -I"$repo/tests/ui-render" -I"$json" -I"$repo/tests/ui-adapter" -I"$repo/test/stubs" -I"$repo/src" -I"$gfx" \
  "$repo/tests/ui-render/runtime-test.cpp" "$repo/src/LocalUiRuntime.cpp" "$repo/src/LocalUiRenderer.cpp" \
  "$repo/src/LocalUi.cpp" "$repo/src/LocalControlPolicy.cpp" "$repo/src/ShadeRequests.cpp" "$repo/src/ShadeMotion.cpp" "$repo/src/SomfyFrame.cpp" "$repo/src/StatusDisplay.cpp" \
  "$repo/src/LocalSettingsForm.cpp" "$repo/src/LocalSettingsRuntime.cpp" "$repo/src/LocalSettingsMemory.cpp" "$repo/src/LocalSettingsRenderer.cpp" \
  "$repo/src/LocalWifiScan.cpp" "$repo/src/LocalWifiScanRuntime.cpp" "$repo/src/WifiScanFeed.cpp" "$repo/src/LocalReceive.cpp" "$repo/src/LocalLearnCandidates.cpp" "$repo/src/LocalDiagnostics.cpp" "$repo/src/LocalDiagnosticsRuntime.cpp" "$repo/src/LocalSystem.cpp" "$repo/src/LocalSystemRuntime.cpp" "$repo/src/LocalAdministration.cpp" "$repo/src/LocalAdvanced.cpp" "$scratch/configuration-owner.o" "$repo/src/LocalOrder.cpp" "$repo/src/LocalEntityForm.cpp" "$repo/src/LocalEntityRenderer.cpp" "$repo/src/EntityFields.cpp" "$repo/src/ProtoEdits.cpp" \
  "$repo/src/ProtoSettings.cpp" "$repo/src/ProtoParse.cpp" "$repo/src/ProtoSchema.cpp" "$repo/src/JsonFormatter.cpp" "$gfx/Adafruit_GFX.cpp" \
  -o "$scratch/runtime"
"$scratch/runtime" "$out"
"${CXX:-c++}" -std=c++17 -O2 -DESPSOMFY_LOCAL_UI -DUI_RUNTIME_HOST_TEST \
  -I"$repo/tests/ui-render" -I"$repo/src" \
  "$repo/tests/ui-render/capture-test.cpp" "$repo/src/LocalUiCapture.cpp" -o "$scratch/capture"
"$scratch/capture"
bash "$repo/tools/test-local-ui-adapter.sh"
bash "$repo/tools/test-local-control-queue.sh"
python3 "$repo/tools/local_ui_previews.py" "$out"

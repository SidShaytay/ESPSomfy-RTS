#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

// Pure compatibility/trust-boundary policy, shared by the worker and host tests.
namespace GitUpdatePolicy {
constexpr const char *BOARD = "lilygo-t-embed-cc1101-plus";
constexpr const char *LAYOUT = "default_8MB";
constexpr const char *REPOSITORY = "SidShaytay/ESPSomfy-RTS";
constexpr const char *FIRMWARE_ASSET = "firmware-lilygo-t-embed-cc1101-plus.bin";
constexpr const char *MANIFEST_ASSET = "update-lilygo-t-embed-cc1101-plus.json";
constexpr uint32_t FLASH_BYTES = 16777216;
constexpr uint32_t PSRAM_BYTES = 8388608;
constexpr uint32_t APP_BYTES = 0x330000;
struct Manifest {
  uint32_t schema = 0, flashBytes = 0, psramBytes = 0, size = 0;
  std::string board, layout, appCompat, version, sourceSha, asset, sha256, channel;
};
const char *validate(const Manifest &manifest, const char *installedApp);
bool newer(const char *available, const char *installed);
bool allowedUrl(const char *url);
std::string identity(const Manifest &manifest);
bool hasIdentity(const uint8_t *data, size_t size, const Manifest &manifest);
}

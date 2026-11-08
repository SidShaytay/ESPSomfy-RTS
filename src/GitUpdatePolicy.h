#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "GitUpdateProfile.h"

// Pure compatibility/trust-boundary policy, shared by the worker and host tests.
namespace GitUpdatePolicy {
constexpr const char *BOARD = GitUpdateProfile::board;
constexpr const char *LAYOUT = GitUpdateProfile::layout;
constexpr const char *REPOSITORY = "SidShaytay/ESPSomfy-RTS";
constexpr const char *FIRMWARE_ASSET = GitUpdateProfile::firmwareAsset;
constexpr const char *MANIFEST_ASSET = GitUpdateProfile::manifestAsset;
constexpr uint32_t FLASH_BYTES = GitUpdateProfile::flashBytes;
constexpr uint32_t PSRAM_BYTES = GitUpdateProfile::psramBytes;
constexpr uint32_t APP_BYTES = GitUpdateProfile::appBytes;
struct Manifest {
  uint32_t features = 0;
  std::vector<std::string> compatibleBoards;
  uint32_t schema = 0, flashBytes = 0, psramBytes = 0, size = 0;
  std::string board, layout, appCompat, version, sourceSha, asset, sha256, channel, bootloaderSha;
};
enum class CheckResult : uint8_t { compatible, schema, hardware, assets, artifact, range, digest, version };
CheckResult check(const Manifest &manifest, const char *installedApp);
const char *reason(CheckResult result);
const char *validate(const Manifest &manifest, const char *installedApp);
bool stableVersion(const char *version);
bool newer(const char *available, const char *installed);
bool allowedUrl(const char *url);
std::string identity(const Manifest &manifest);
bool hasIdentity(const uint8_t *data, size_t size, const Manifest &manifest);
}

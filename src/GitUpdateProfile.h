#pragma once
#include <cstdint>
#if defined(ESPSOMFY_GITHUB_UPDATES) && !defined(GIT_UPDATE_HOST_TEST)
#include <sdkconfig.h>
#endif

// Firmware values come from the reviewed update-profiles.json build profile.
// The native policy tests retain the original Plus fixture without enabling
// any firmware downloader. Enabled firmware never guesses a generic profile.
#if defined(ESPSOMFY_GITHUB_UPDATES)
#if !defined(ESPSOMFY_UPDATE_PROFILE) || !defined(ESPSOMFY_UPDATE_LAYOUT) || \
  !defined(ESPSOMFY_UPDATE_FLASH_BYTES) || !defined(ESPSOMFY_UPDATE_PSRAM_BYTES) || \
  !defined(ESPSOMFY_UPDATE_APP_BYTES) || !defined(ESPSOMFY_UPDATE_APP0_ADDRESS) || \
  !defined(ESPSOMFY_UPDATE_APP1_ADDRESS) || !defined(ESPSOMFY_UPDATE_FS_ADDRESS) || \
  !defined(ESPSOMFY_UPDATE_FS_BYTES) || !defined(ESPSOMFY_UPDATE_NVS_ADDRESS) || \
  !defined(ESPSOMFY_UPDATE_NVS_BYTES) || !defined(ESPSOMFY_UPDATE_CHIP_ID)
#error "GitHub updates require an explicit complete reviewed board profile"
#endif
#else
#define ESPSOMFY_UPDATE_PROFILE "lilygo-t-embed-cc1101-plus"
#define ESPSOMFY_UPDATE_LAYOUT "default_8MB"
#define ESPSOMFY_UPDATE_FLASH_BYTES 16777216
#define ESPSOMFY_UPDATE_PSRAM_BYTES 8388608
#define ESPSOMFY_UPDATE_APP_BYTES 0x330000
#define ESPSOMFY_UPDATE_APP0_ADDRESS 0x10000
#define ESPSOMFY_UPDATE_APP1_ADDRESS 0x340000
#define ESPSOMFY_UPDATE_FS_ADDRESS 0x670000
#define ESPSOMFY_UPDATE_FS_BYTES 0x180000
#define ESPSOMFY_UPDATE_NVS_ADDRESS 0x9000
#define ESPSOMFY_UPDATE_NVS_BYTES 0x5000
#define ESPSOMFY_UPDATE_CHIP_ID 9
#endif

namespace GitUpdateProfile {
constexpr const char *board=ESPSOMFY_UPDATE_PROFILE;
constexpr const char *layout=ESPSOMFY_UPDATE_LAYOUT;
constexpr const char *firmwareAsset="firmware-" ESPSOMFY_UPDATE_PROFILE ".bin";
constexpr const char *manifestAsset="update-" ESPSOMFY_UPDATE_PROFILE ".json";
constexpr uint32_t flashBytes=ESPSOMFY_UPDATE_FLASH_BYTES;
constexpr uint32_t psramBytes=ESPSOMFY_UPDATE_PSRAM_BYTES;
constexpr uint32_t appBytes=ESPSOMFY_UPDATE_APP_BYTES;
constexpr uint32_t app0Address=ESPSOMFY_UPDATE_APP0_ADDRESS;
constexpr uint32_t app1Address=ESPSOMFY_UPDATE_APP1_ADDRESS;
constexpr uint32_t fsAddress=ESPSOMFY_UPDATE_FS_ADDRESS;
constexpr uint32_t fsBytes=ESPSOMFY_UPDATE_FS_BYTES;
constexpr uint32_t nvsAddress=ESPSOMFY_UPDATE_NVS_ADDRESS;
constexpr uint32_t nvsBytes=ESPSOMFY_UPDATE_NVS_BYTES;
constexpr uint16_t chipId=ESPSOMFY_UPDATE_CHIP_ID;
#if defined(ESPSOMFY_GITHUB_UPDATES) && !defined(GIT_UPDATE_HOST_TEST)
#if defined(CONFIG_IDF_TARGET_ESP32S3)
static_assert(chipId==9,"Update profile chip differs from compiled ESP32-S3 SDK");
#elif defined(CONFIG_IDF_TARGET_ESP32P4)
static_assert(chipId==18,"Update profile chip differs from compiled ESP32-P4 SDK");
#else
#error "Verified update profiles require a reviewed ESP32-S3 or ESP32-P4 target"
#endif
#endif
static_assert(psramBytes>=8u*1024*1024,"Verified updates require at least 8 MiB PSRAM");
static_assert(chipId==9 || chipId==18,"Only reviewed ESP32-S3 and ESP32-P4 image formats are enabled");
static_assert(app0Address>=nvsAddress+nvsBytes && app1Address>=app0Address+appBytes,
  "Update application slots must not overlap NVS or each other");
static_assert(fsAddress>=app1Address+appBytes && flashBytes>=fsAddress+fsBytes,
  "Update filesystem must fit after the inactive application slot");
}

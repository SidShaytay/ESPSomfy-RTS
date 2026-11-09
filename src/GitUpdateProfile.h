#pragma once
#include <cstdint>
// Compatibility policy only. This does not enable a downloader or installer.
// One exact supported hardware family; generic builds do not acquire support.
namespace GitUpdateProfile {
constexpr const char *board="lilygo-t-embed-cc1101";
constexpr const char *plusBoard="lilygo-t-embed-cc1101-plus";
constexpr const char *layout="default_8MB";
constexpr const char *firmwareAsset="firmware-lilygo-t-embed-cc1101.bin";
constexpr const char *manifestAsset="update-lilygo-t-embed-cc1101.json";
constexpr uint32_t flashBytes=0x1000000, psramBytes=0x800000, appBytes=0x330000;
constexpr uint32_t app0Address=0x10000, app1Address=0x340000;
constexpr uint32_t fsAddress=0x670000, fsBytes=0x180000;
constexpr uint32_t nvsAddress=0x9000, nvsBytes=0x5000;
constexpr uint16_t chipId=9;
constexpr const char *assetsMode="bundled";
constexpr uint8_t features=15; // display1, interactive UI2, local commands4, bundled web8
}

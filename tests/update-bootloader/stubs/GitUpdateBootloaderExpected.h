#pragma once
#include <cstddef>
#include <cstdint>
constexpr size_t ESPSOMFY_BOOTLOADER_BYTES = 1025;
// Test hash provider sums bytes, so 1025 one-bytes produce 0x401.
// Real firmware uses the SDK's SHA256 and generator tests verify its digest.
constexpr uint8_t ESPSOMFY_BOOTLOADER_SHA256[32] = {1, 4};
constexpr char ESPSOMFY_BOOTLOADER_SHA256_HEX[] = "0104000000000000000000000000000000000000000000000000000000000000";

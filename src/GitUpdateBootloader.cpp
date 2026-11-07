#include "GitUpdateBootloader.h"

#ifdef ESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS
#include "GitUpdateBootloaderExpected.h"
#include <esp_flash.h>
#include <mbedtls/sha256.h>
#include <atomic>
#include <cstring>

namespace {
std::atomic<int> attestation{-1};
bool readAndVerify() {
  static_assert(ESPSOMFY_BOOTLOADER_BYTES > 0 && ESPSOMFY_BOOTLOADER_BYTES <= 0x8000,
    "bootloader identity must not overlap partitions");
  mbedtls_sha256_context hash;
  mbedtls_sha256_init(&hash);
  bool valid = mbedtls_sha256_starts(&hash, 0) == 0;
  // Both the calling task's stack and this aligned scratch buffer are internal;
  // raw reads also fail closed on flash-encrypted unsupported configurations.
  alignas(4) uint8_t chunk[512];
  for(size_t offset = 0; valid && offset < ESPSOMFY_BOOTLOADER_BYTES;) {
    const size_t remain = ESPSOMFY_BOOTLOADER_BYTES - offset;
    const size_t count = remain < sizeof(chunk) ? remain : sizeof(chunk);
    valid = esp_flash_read(nullptr, chunk, offset, count) == ESP_OK &&
      mbedtls_sha256_update(&hash, chunk, count) == 0;
    offset += count;
  }
  uint8_t actual[32];
  valid = valid && mbedtls_sha256_finish(&hash, actual) == 0 &&
    memcmp(actual, ESPSOMFY_BOOTLOADER_SHA256, sizeof(actual)) == 0;
  mbedtls_sha256_free(&hash);
  return valid;
}
}
bool gitUpdateBootloaderMatches() {
  int result = attestation.load();
  if(result < 0) {
    result = readAndVerify() ? 1 : 0;
    attestation.store(result);
  }
  return result == 1;
}
const char *gitUpdateBootloaderDigest() { return ESPSOMFY_BOOTLOADER_SHA256_HEX; }
#else
const char *gitUpdateBootloaderDigest() { return ""; }
bool gitUpdateBootloaderMatches() { return false; }
#endif

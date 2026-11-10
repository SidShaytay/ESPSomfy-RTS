#include "GitUpdateBootloader.h"

#ifdef ESPSOMFY_LILYGO_T_EMBED_CC1101
#include "GitUpdateBootloaderExpected.h"
#include <esp_flash.h>
#include <esp_flash_encrypt.h>
#include <mbedtls/sha256.h>
#include <atomic>
#include <cstring>

namespace {
std::atomic<int> attestation{-1};
UpdateAttestation readAndVerify(UpdateAttestationControl control) {
  if(control.stopped()) return UpdateAttestation::interrupted;
  if(esp_flash_encryption_enabled()) return UpdateAttestation::encrypted;
  static_assert(ESPSOMFY_BOOTLOADER_BYTES > 0 && ESPSOMFY_BOOTLOADER_BYTES <= 0x8000 &&
    ESPSOMFY_BOOTLOADER_ADDRESS == 0,
    "shared bootloader identity must not overlap partitions");
  mbedtls_sha256_context hash;
  mbedtls_sha256_init(&hash);
  UpdateAttestation result = UpdateAttestation::verified;
  if(mbedtls_sha256_starts(&hash, 0) != 0) result=UpdateAttestation::hashError;
  alignas(4) uint8_t chunk[512];
  for(size_t offset=0; result == UpdateAttestation::verified && offset<ESPSOMFY_BOOTLOADER_BYTES;) {
    if(control.stopped()) { result=UpdateAttestation::interrupted;break; }
    const size_t remain=ESPSOMFY_BOOTLOADER_BYTES-offset;
    const size_t count=remain<sizeof(chunk) ? remain : sizeof(chunk);
    if(esp_flash_read(nullptr,chunk,ESPSOMFY_BOOTLOADER_ADDRESS+offset,count) != ESP_OK)
      result=UpdateAttestation::readError;
    else if(mbedtls_sha256_update(&hash,chunk,count) != 0)
      result=UpdateAttestation::hashError;
    offset+=count;
    control.cooperate();
    if(control.stopped()) result=UpdateAttestation::interrupted;
  }
  uint8_t actual[32];
  if(result == UpdateAttestation::verified) {
    if(mbedtls_sha256_finish(&hash,actual) != 0) result=UpdateAttestation::hashError;
    else if(memcmp(actual,ESPSOMFY_BOOTLOADER_SHA256,sizeof(actual)) != 0) result=UpdateAttestation::mismatch;
  }
  mbedtls_sha256_free(&hash);
  return result;
}
}
UpdateAttestation gitUpdateBootloaderStatus(UpdateAttestationControl control) {
  if(control.stopped()) return UpdateAttestation::interrupted;
  int result=attestation.load();
  if(result<0) {
    result=static_cast<int>(readAndVerify(control));
    if(result!=static_cast<int>(UpdateAttestation::interrupted)) attestation.store(result);
  }
  return static_cast<UpdateAttestation>(result);
}
bool gitUpdateBootloaderMatches() { return updateAttested(gitUpdateBootloaderStatus()); }
extern "C" const char espsomfyBootloaderIdentity[] = ESPSOMFY_BOOTLOADER_ID;
const char *gitUpdateBootloaderIdentity() { return espsomfyBootloaderIdentity; }
const char *gitUpdateBootloaderDigest() { return ESPSOMFY_BOOTLOADER_SHA256_HEX; }
#else
UpdateAttestation gitUpdateBootloaderStatus(UpdateAttestationControl) { return UpdateAttestation::unsupported; }
const char *gitUpdateBootloaderIdentity() { return ""; }
const char *gitUpdateBootloaderDigest() { return ""; }
bool gitUpdateBootloaderMatches() { return false; }
#endif

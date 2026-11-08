#pragma once
#include <cstdint>
// Pure result vocabulary. Missing/unreadable prerequisites never mean permission
// to provision, install or recover a device.
enum class UpdateAttestation : uint8_t {
  verified, unsupported, missing, invalid, bounds, readError, hashError,
  mismatch, encrypted, directoryError, unexpectedAsset
};
inline bool updateAttested(UpdateAttestation result) { return result == UpdateAttestation::verified; }

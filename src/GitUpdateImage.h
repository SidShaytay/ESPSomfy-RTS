#pragma once
#include "GitUpdatePolicy.h"
#include "GitUpdateAttestation.h"
#include <cstddef>
#include <cstdint>
namespace GitUpdateImage {
enum class Result : uint8_t { verified, bounds, header, segments, checksum, hashError, digest, description, identity, bootloader, bundle, interrupted };
struct Identity {
  char sourceSha[41]={},version[48]={},appCompat[72]={},appVersion[15]={},imageSha[65]={};
};
// Verifies only staged bytes. A null expected manifest is the explicitly
// user-selected manual path, not authenticated GitHub provenance. bootloaderIdentity
// must identify the separately attested installed bootloader; no bootloader is written.
Result verify(const uint8_t *bytes,size_t size,const char *bootloaderIdentity,
              const GitUpdatePolicy::Manifest *expected,Identity &identity,
              UpdateAttestationControl control={});
}

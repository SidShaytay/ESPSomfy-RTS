#pragma once
#include "GitUpdateAttestation.h"
#include <cstddef>
#include <cstdint>
namespace BundledWeb {
struct Asset {
  const char *path;
  const char *mime;
  const uint8_t *data;
  size_t size;
  bool gzip;
  const char *sha256;
};
// Immutable pointers into the current firmware image; never reads LittleFS.
const Asset *find(const char *path);
const char *sourceVersion();
UpdateAttestation verify(const char *compatibility,UpdateAttestationControl control={});
}

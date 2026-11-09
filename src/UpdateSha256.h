#pragma once
#include "GitUpdateAttestation.h"
#include <cstddef>
#include <cstdint>
// Cooperative SHA256 over bounded caller-owned bytes. No input staging/copy.
UpdateAttestation updateSha256(const uint8_t *bytes,size_t size,char (&hex)[65],UpdateAttestationControl control={});

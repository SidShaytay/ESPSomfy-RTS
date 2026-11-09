#pragma once
#include <cstdint>

enum class GitUpdateExternalOwner : uint8_t { manual, restore, counter, restart };
struct GitUpdateOwnerToken {
  uint64_t generation=0;
  GitUpdateExternalOwner kind=GitUpdateExternalOwner::manual;
};

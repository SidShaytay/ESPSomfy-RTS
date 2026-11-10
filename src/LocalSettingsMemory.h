#pragma once
#ifdef ESPSOMFY_LOCAL_UI
#include "LocalJsonAllocator.h"
namespace LocalUi { namespace Settings {
// Budget counts requested allocation bytes including our aligned headers.
// The SDK adds its own heap metadata/alignment overhead; reserve headroom too.
constexpr size_t runtimeMemoryBudget=4096;
JsonAllocator &runtimeMemory();
} }
#endif

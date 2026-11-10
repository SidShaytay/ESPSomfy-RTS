#pragma once
#ifdef ESPSOMFY_LOCAL_UI
#include "LocalAdvanced.h"
namespace LocalUi {
namespace Advanced {
// Loop-owned metadata and explicitly confirmed sensor backend. No retained model/frame copy.
Backend &runtimeBackend();
} // namespace Advanced
} // namespace LocalUi
#endif

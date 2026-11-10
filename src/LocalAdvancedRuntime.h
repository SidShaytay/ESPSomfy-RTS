#pragma once
#ifdef ESPSOMFY_LOCAL_UI
#include "LocalAdvanced.h"
namespace LocalUi {
namespace Advanced {
// Loop-owned metadata and explicit remote backend. No retained model/frame copy.
Backend &runtimeBackend();
} // namespace Advanced
} // namespace LocalUi
#endif

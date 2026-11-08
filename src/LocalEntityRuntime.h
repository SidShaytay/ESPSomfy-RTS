#pragma once
#ifdef ESPSOMFY_LOCAL_UI
#include "LocalEntityForm.h"
namespace LocalUi {
namespace EntityAdmin {
// Loop-owned; parent retains one Form and injects its bounded wiping PSRAM
// allocator. No allocation or I/O is performed just by obtaining this backend.
Backend &runtimeBackend();
} // namespace EntityAdmin
} // namespace LocalUi
#endif

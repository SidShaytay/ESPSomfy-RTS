#pragma once
#ifdef ESPSOMFY_LOCAL_UI
#include "LocalEntityForm.h"
namespace LocalUi {
namespace EntityAdmin {
// Loop-owned; parent retains one Form and injects its bounded wiping PSRAM
// allocator. One prepared Form owns the backend's bounded exact target capture;
// another prepare replaces that capture. Read defaults with sourceId0; an
// applied edit adopts the returned ID/tombstone for file-only retry. No model
// copy, allocation or I/O is performed just by obtaining this backend.
Backend &runtimeBackend();
} // namespace EntityAdmin
} // namespace LocalUi
#endif

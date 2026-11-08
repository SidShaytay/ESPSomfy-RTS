#pragma once
#ifdef ESPSOMFY_LOCAL_UI
#include "LocalSettingsForm.h"
#include "LocalSettingsMemory.h"
namespace LocalUi { namespace Settings {
// Loop-owned single form service. Documents must be cleared before releasing
// their owning Form; all draft and expanded validation allocations share a cap.
Backend &runtimeBackend();
} }
#endif

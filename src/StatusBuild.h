#pragma once
#include "BoardProfile.h"
#include <stdio.h>

namespace StatusPanel {
// Shared by the real status snapshot and host checks, including build labels.
inline void buildIdentity(char *out, size_t size, const char *version) {
#ifdef ESPSOMFY_LOCAL_UI
  snprintf(out, size, "%s UI %s", BoardProfile::name, version);
#else
  snprintf(out, size, "%s LCD %s", BoardProfile::name, version);
#endif
}
}

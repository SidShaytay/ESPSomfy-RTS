#include "GitUpdateIdentity.h"
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101)
#include "LilygoBuild.h"
#ifndef ESPSOMFY_SOURCE_SHA
#error "Shared updater policy requires app_version.py build identity"
#endif
// app_version.py retains this named symbol through section garbage collection,
// even while the network worker is deliberately inactive.
extern "C" const char espsomfyUpdateIdentity[] =
  "ESPSOMFY_UPDATE_ID:3|lilygo-t-embed-cc1101|default_8MB|"
  ESPSOMFY_SOURCE_SHA "|" ESPSOMFY_RELEASE_TAG "|bundled|" ESPSOMFY_APP_COMPAT;
const char *gitUpdateIdentity() { return espsomfyUpdateIdentity; }
#else
const char *gitUpdateIdentity() { return "unsupported"; }
#endif

#include "GitUpdateIdentity.h"

#ifdef ESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS
#ifndef ESPSOMFY_SOURCE_SHA
#error "The Plus updater requires app_version.py build identity"
#endif
const char *gitUpdateIdentity() {
  return "ESPSOMFY_UPDATE_ID:1|lilygo-t-embed-cc1101-plus|default_8MB|"
    ESPSOMFY_SOURCE_SHA "|" ESPSOMFY_RELEASE_TAG "|" ESPSOMFY_APP_COMPAT;
}
#else
const char *gitUpdateIdentity() { return "unsupported"; }
#endif

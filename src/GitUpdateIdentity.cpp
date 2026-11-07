#include "GitUpdateIdentity.h"

#ifdef ESPSOMFY_GITHUB_UPDATES
#ifndef ESPSOMFY_SOURCE_SHA
#error "The board-profile updater requires app_version.py build identity"
#endif
const char *gitUpdateIdentity() {
  return "ESPSOMFY_UPDATE_ID:1|" ESPSOMFY_UPDATE_PROFILE "|" ESPSOMFY_UPDATE_LAYOUT "|"
    ESPSOMFY_SOURCE_SHA "|" ESPSOMFY_RELEASE_TAG "|" ESPSOMFY_APP_COMPAT;
}
#else
const char *gitUpdateIdentity() { return "unsupported"; }
#endif

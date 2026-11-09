#include "GitUpdateAssets.h"
#if defined(ESPSOMFY_BUNDLED_WEB)
#include "BundledWeb.h"
#ifndef ESPSOMFY_APP_COMPAT
#error "Bundled web requires its compiled exact-byte compatibility digest"
#endif
UpdateAttestation gitUpdateAssetsStatus(UpdateAttestationControl control) {
  return BundledWeb::verify(ESPSOMFY_APP_COMPAT,control);
}
bool gitUpdateAssetsMatch(){return updateAttested(gitUpdateAssetsStatus());}
#else
UpdateAttestation gitUpdateAssetsStatus(UpdateAttestationControl control) {
  return control.stopped()?UpdateAttestation::interrupted:UpdateAttestation::unsupported;
}
bool gitUpdateAssetsMatch(){return false;}
#endif

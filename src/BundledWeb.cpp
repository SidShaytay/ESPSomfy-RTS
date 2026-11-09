#if defined(ESPSOMFY_BUNDLED_WEB)
#include "BundledWeb.h"
#include "HttpAccess.h"
#include "UpdateSha256.h"
#include <cstring>
#include "BundledWebData.h"
namespace BundledWeb {
namespace {
constexpr size_t count=sizeof(generated::table)/sizeof(generated::table[0]);
}
const Asset *find(const char *path) {
  if(!isWebAsset(path))return nullptr;
  if(std::strcmp(path,"/")==0)path="/index.html";
  for(const auto &asset:generated::table)if(std::strcmp(path,asset.path)==0)return &asset;
  return nullptr;
}
const char *sourceVersion(){return generated::version;}
UpdateAttestation verify(const char *compatibility,UpdateAttestationControl control) {
  if(control.stopped())return UpdateAttestation::interrupted;
  if(!compatibility || std::strlen(compatibility)!=71 || std::strncmp(compatibility,"sha256:",7))return UpdateAttestation::invalid;
  for(size_t i=7;i<71;++i)if(!((compatibility[i]>='0'&&compatibility[i]<='9')||(compatibility[i]>='a'&&compatibility[i]<='f')))return UpdateAttestation::invalid;
  if(!count || count>128)return UpdateAttestation::bounds;
  char digest[65];
  auto result=updateSha256(generated::blob+generated::manifestOffset,generated::manifestSize,digest,control);
  if(result!=UpdateAttestation::verified)return result;
  if(std::strcmp(digest,compatibility+7)!=0)return UpdateAttestation::mismatch;
  for(const auto &asset:generated::table) {
    result=updateSha256(asset.data,asset.size,digest,control);
    if(result!=UpdateAttestation::verified)return result;
    if(std::strcmp(digest,asset.sha256)!=0)return UpdateAttestation::mismatch;
  }
  return UpdateAttestation::verified;
}
}
#endif

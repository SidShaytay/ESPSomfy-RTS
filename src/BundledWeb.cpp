#if defined(ESPSOMFY_BUNDLED_WEB)
#include "BundledWeb.h"
#include "HttpAccess.h"
#include "TaskWdt.h"
#include <mbedtls/sha256.h>
#include <cstring>
#include "BundledWebData.h"
namespace BundledWeb {
namespace {
constexpr size_t count=sizeof(generated::table)/sizeof(generated::table[0]);
UpdateAttestation hash(const uint8_t *data,size_t size,char (&hex)[65],UpdateAttestationControl control) {
  if(control.stopped())return UpdateAttestation::interrupted;
  mbedtls_sha256_context state;mbedtls_sha256_init(&state);
  auto result=mbedtls_sha256_starts(&state,0)==0?UpdateAttestation::verified:UpdateAttestation::hashError;
  for(size_t offset=0;result==UpdateAttestation::verified && offset<size;) {
    if(control.stopped()){result=UpdateAttestation::interrupted;break;}
    const size_t chunk=size-offset<1024?size-offset:1024;
    if(mbedtls_sha256_update(&state,data+offset,chunk)!=0)result=UpdateAttestation::hashError;
    offset+=chunk;resetTaskWdt();control.cooperate();
    if(control.stopped())result=UpdateAttestation::interrupted;
  }
  uint8_t digest[32];
  if(result==UpdateAttestation::verified && mbedtls_sha256_finish(&state,digest)!=0)result=UpdateAttestation::hashError;
  mbedtls_sha256_free(&state);
  if(result==UpdateAttestation::verified) {
    constexpr char digits[]="0123456789abcdef";
    for(size_t i=0;i<32;++i){hex[i*2]=digits[digest[i]>>4];hex[i*2+1]=digits[digest[i]&15];}hex[64]=0;
  }
  return result;
}
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
  auto result=hash(generated::blob+generated::manifestOffset,generated::manifestSize,digest,control);
  if(result!=UpdateAttestation::verified)return result;
  if(std::strcmp(digest,compatibility+7)!=0)return UpdateAttestation::mismatch;
  for(const auto &asset:generated::table) {
    result=hash(asset.data,asset.size,digest,control);
    if(result!=UpdateAttestation::verified)return result;
    if(std::strcmp(digest,asset.sha256)!=0)return UpdateAttestation::mismatch;
  }
  return UpdateAttestation::verified;
}
}
#endif

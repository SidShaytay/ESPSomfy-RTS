#include "UpdateSha256.h"
#include "TaskWdt.h"
#include <mbedtls/sha256.h>
UpdateAttestation updateSha256(const uint8_t *data,size_t size,char (&hex)[65],UpdateAttestationControl control) {
  hex[0]=0;
  if(control.stopped())return UpdateAttestation::interrupted;
  if(!data && size)return UpdateAttestation::invalid;
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
  if(control.stopped())return UpdateAttestation::interrupted;
  if(result==UpdateAttestation::verified) {
    constexpr char digits[]="0123456789abcdef";
    for(size_t i=0;i<32;++i){hex[i*2]=digits[digest[i]>>4];hex[i*2+1]=digits[digest[i]&15];}hex[64]=0;
  }
  return result;
}

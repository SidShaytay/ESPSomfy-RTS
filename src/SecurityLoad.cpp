#include "SecurityLoad.h"
#include <nvs.h>
#include <string.h>
namespace {
template<size_t N> bool text(nvs_handle_t handle,const char *key,char (&out)[N],bool &present) {
  size_t size=0;
  const auto result=nvs_get_str(handle,key,nullptr,&size);
  if(result==ESP_ERR_NVS_NOT_FOUND)return true;
  if(result!=ESP_OK || !size || size>N)return false;
  present=true;
  const size_t expected=size;
  if(nvs_get_str(handle,key,out,&size)!=ESP_OK || size!=expected)return false;
  // Reject missing or embedded terminators rather than accepting a prefix.
  return out[size-1]==0 && memchr(out,0,size)==out+size-1;
}
}
SecurityLoadState readSecurityRecord(SecurityRecord &output) {
  output.clear();SecurityRecord draft;
  nvs_handle_t handle=0;
  const auto opened=nvs_open("SEC",NVS_READONLY,&handle);
  if(opened==ESP_ERR_NVS_NOT_FOUND)return SecurityLoadState::knownEmpty;
  if(opened!=ESP_OK)return SecurityLoadState::failed;
  struct Close {nvs_handle_t handle;~Close(){nvs_close(handle);}} close{handle};
  int8_t type=0;
  const auto mode=nvs_get_i8(handle,"type",&type);
  bool user=false,password=false,pin=false;
  if((mode!=ESP_OK && mode!=ESP_ERR_NVS_NOT_FOUND) ||
     !text(handle,"username",draft.username,user) ||
     !text(handle,"password",draft.password,password) ||
     !text(handle,"pin",draft.pin,pin))return SecurityLoadState::failed;
  if(mode==ESP_ERR_NVS_NOT_FOUND) {
    return user || password || pin ? SecurityLoadState::failed : SecurityLoadState::knownEmpty;
  }
  if(type<0 || type>2 || (type==1 && strlen(draft.pin)!=4) ||
     (type==2 && (!draft.username[0] || !draft.password[0])))return SecurityLoadState::failed;
  draft.type=uint8_t(type);
  output=draft;
  return SecurityLoadState::loaded;
}

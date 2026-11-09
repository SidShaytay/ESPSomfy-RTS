#include "SettingsLoad.h"
#include <string.h>
SettingsLoadReader::SettingsLoadReader(const char *name) {
  const auto read=nvs_open(name,NVS_READONLY,&handle);
  opened=read==ESP_OK;
  if(!opened && read!=ESP_ERR_NVS_NOT_FOUND)value.state=SettingsLoadState::failed;
}
SettingsLoadReader::~SettingsLoadReader(){if(opened)nvs_close(handle);}
SettingsReadField SettingsLoadReader::result(esp_err_t read) {
  if(read==ESP_ERR_NVS_NOT_FOUND)return SettingsReadField::absent;
  if(read!=ESP_OK){value.state=SettingsLoadState::failed;return SettingsReadField::failed;}
  if(value.state!=SettingsLoadState::invalid)value.state=SettingsLoadState::loaded;
  return SettingsReadField::present;
}
SettingsReadField SettingsLoadReader::flag(const char *key,bool &out) {
  uint8_t draft=0;const auto read=u8(key,draft);
  if(read==SettingsReadField::present) {
    if(draft>1){value.state=SettingsLoadState::failed;return SettingsReadField::failed;}
    out=draft==1;
  }
  return read;
}
SettingsReadField SettingsLoadReader::real(const char *key,float &out) {
  if(value.state==SettingsLoadState::failed)return SettingsReadField::failed;
  if(!opened)return SettingsReadField::absent;
  float draft=0;size_t size=sizeof(draft);
  const auto read=result(nvs_get_blob(handle,key,&draft,&size));
  if(read==SettingsReadField::present) {
    if(size!=sizeof(draft)){value.state=SettingsLoadState::failed;return SettingsReadField::failed;}
    out=draft; // Preserve nonfinite originals; semantic validation owns usability.
  }
  return read;
}
SettingsReadField SettingsLoadReader::readText(const char *key,char *out,size_t capacity) {
  if(value.state==SettingsLoadState::failed)return SettingsReadField::failed;
  if(!opened)return SettingsReadField::absent;
  size_t size=0;const auto read=result(nvs_get_str(handle,key,nullptr,&size));
  if(read!=SettingsReadField::present)return read;
  if(!size || size>capacity){value.state=SettingsLoadState::failed;return SettingsReadField::failed;}
  struct Draft {char bytes[65]={};~Draft(){volatile char *p=bytes;for(size_t i=0;i<sizeof(bytes);++i)p[i]=0;}} draft;
  const auto expected=size;
  if(nvs_get_str(handle,key,draft.bytes,&size)!=ESP_OK || size!=expected ||
     draft.bytes[size-1]!=0 || memchr(draft.bytes,0,size)!=draft.bytes+size-1) {
    value.state=SettingsLoadState::failed;return SettingsReadField::failed;
  }
  volatile char *previous=out;for(size_t i=0;i<capacity;++i)previous[i]=0;
  memcpy(out,draft.bytes,size);
  return SettingsReadField::present;
}

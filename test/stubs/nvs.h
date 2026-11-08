#pragma once
#include <stdint.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>
using esp_err_t = int;
using nvs_handle_t = uint32_t;
constexpr esp_err_t ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_NVS_NOT_FOUND = 1;
constexpr int NVS_READWRITE = 1;
namespace FakeNvs {
enum class Type { i8, u8, i16, text, blob };
struct Value { Type type; std::vector<uint8_t> bytes; };
inline std::map<std::string,Value> values;
inline std::map<nvs_handle_t,std::string> handles;
inline int opens, sets, commits, reads, closes;
inline int failOpen, failSet, failCommit, failRead, corruptRead;
inline void reset() {
  values.clear(); handles.clear();
  opens=sets=commits=reads=closes=0;
  failOpen=failSet=failCommit=failRead=corruptRead=0;
}
inline esp_err_t set(nvs_handle_t h,const char *key,Type type,const void *data,size_t size) {
  if(++sets == failSet || !handles.count(h)) return ESP_FAIL;
  auto p=static_cast<const uint8_t *>(data);
  values[handles[h]+"/"+key]={type,{p,p+size}};
  return ESP_OK;
}
inline esp_err_t get(nvs_handle_t h,const char *key,Type type,void *data,size_t *size) {
  if(++reads == failRead || !handles.count(h)) return ESP_FAIL;
  auto it=values.find(handles[h]+"/"+key);
  if(it==values.end() || it->second.type!=type) return ESP_ERR_NVS_NOT_FOUND;
  size_t len=it->second.bytes.size();
  if(data && *size<len) return ESP_FAIL;
  if(data) {
    memcpy(data,it->second.bytes.data(),len);
    if(reads==corruptRead && len) static_cast<uint8_t *>(data)[0]^=1;
  }
  *size=len;
  return ESP_OK;
}
}
inline esp_err_t nvs_open(const char *space,int,nvs_handle_t *h) {
  if(++FakeNvs::opens==FakeNvs::failOpen) return ESP_FAIL;
  *h=FakeNvs::opens;
  FakeNvs::handles[*h]=space;
  return ESP_OK;
}
inline void nvs_close(nvs_handle_t h) { FakeNvs::closes++; FakeNvs::handles.erase(h); }
inline esp_err_t nvs_commit(nvs_handle_t h) {
  return ++FakeNvs::commits==FakeNvs::failCommit || !FakeNvs::handles.count(h) ? ESP_FAIL : ESP_OK;
}
#define FAKE_NVS_VALUE(name,type,kind) \
inline esp_err_t nvs_set_##name(nvs_handle_t h,const char *key,type value) { \
  return FakeNvs::set(h,key,FakeNvs::Type::kind,&value,sizeof(value)); } \
inline esp_err_t nvs_get_##name(nvs_handle_t h,const char *key,type *value) { \
  size_t size=sizeof(*value); return FakeNvs::get(h,key,FakeNvs::Type::kind,value,&size); }
FAKE_NVS_VALUE(i8,int8_t,i8)
FAKE_NVS_VALUE(u8,uint8_t,u8)
FAKE_NVS_VALUE(i16,int16_t,i16)
#undef FAKE_NVS_VALUE
inline esp_err_t nvs_set_str(nvs_handle_t h,const char *key,const char *value) {
  return FakeNvs::set(h,key,FakeNvs::Type::text,value,strlen(value)+1);
}
inline esp_err_t nvs_get_str(nvs_handle_t h,const char *key,char *value,size_t *size) {
  return FakeNvs::get(h,key,FakeNvs::Type::text,value,size);
}
inline esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *value,size_t size) {
  return FakeNvs::set(h,key,FakeNvs::Type::blob,value,size);
}
inline esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *value,size_t *size) {
  return FakeNvs::get(h,key,FakeNvs::Type::blob,value,size);
}

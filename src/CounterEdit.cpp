#include "CounterEdit.h"
#include <nvs.h>
#include <stdio.h>
namespace {
CounterEditResult persistCounter(uint32_t address,uint16_t code,bool observed,uint16_t aliasHighwater) {
  if(!address || address>0xffffff)return {};
  char key[11];snprintf(key,sizeof(key),"_%lu",static_cast<unsigned long>(address));
  nvs_handle_t handle;
  if(nvs_open("ShadeCodes",NVS_READWRITE,&handle)!=ESP_OK)return {};
  if(observed) {
    uint16_t stored=0;
    const auto read=nvs_get_u16(handle,key,&stored);
    // Only exact absence is zero. A failed read never authorizes a write.
    if(read!=ESP_OK && read!=ESP_ERR_NVS_NOT_FOUND){nvs_close(handle);return {};}
    if(code<stored || code<aliasHighwater){nvs_close(handle);return {CounterEditState::stale};}
  }
  // Once a set was attempted, conservatively report uncertainty for any
  // failure. Never write the previous value as a supposed rollback.
  bool saved=nvs_set_u16(handle,key,code)==ESP_OK;
  if(saved)saved=nvs_commit(handle)==ESP_OK;
  uint16_t readback=0;
  if(saved)saved=nvs_get_u16(handle,key,&readback)==ESP_OK && readback==code;
  nvs_close(handle);
  return {saved?CounterEditState::saved:CounterEditState::uncertain};
}
}
CounterEditResult persistExplicitCounter(uint32_t address,uint16_t code) {
  return persistCounter(address,code,false,0);
}
CounterEditResult persistObservedCounter(uint32_t address,uint16_t code,uint16_t aliasHighwater) {
  return persistCounter(address,code,true,aliasHighwater);
}

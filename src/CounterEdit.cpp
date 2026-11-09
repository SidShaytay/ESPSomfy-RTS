#include "CounterEdit.h"
#include <nvs.h>
#include <stdio.h>
bool readExplicitCounter(uint32_t address,StoredCounter &out) {
  if(!address || address>0xffffff)return false;
  char key[11];snprintf(key,sizeof(key),"_%lu",static_cast<unsigned long>(address));
  nvs_handle_t handle;
  StoredCounter current;
  const auto opened=nvs_open("ShadeCodes",NVS_READONLY,&handle);
  if(opened==ESP_OK) {
    const auto read=nvs_get_u16(handle,key,&current.code);
    nvs_close(handle);
    if(read!=ESP_OK && read!=ESP_ERR_NVS_NOT_FOUND)return false;
    current.present=read==ESP_OK;
    if(!current.present)current.code=0;
  } else if(opened!=ESP_ERR_NVS_NOT_FOUND)return false;
  out=current;return true;
}
namespace {
CounterEditResult persistCounter(uint32_t address,uint16_t code,bool observed,uint16_t aliasHighwater) {
  if(!address || address>0xffffff)return {};
  if(observed && code<aliasHighwater)return {CounterEditState::stale};
  char key[11];snprintf(key,sizeof(key),"_%lu",static_cast<unsigned long>(address));
  nvs_handle_t handle;
  if(observed) {
    // Preflight cannot create a namespace merely to reject a received code.
    // Caller retains the same identity reservation across both handles.
    StoredCounter stored;
    if(!readExplicitCounter(address,stored))return {};
    if(code<stored.code)return {CounterEditState::stale};
  }
  if(nvs_open("ShadeCodes",NVS_READWRITE,&handle)!=ESP_OK)return {};
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

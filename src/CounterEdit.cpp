#include "CounterEdit.h"
#include <nvs.h>
#include <stdio.h>
CounterEditResult persistExplicitCounter(uint32_t address,uint16_t code) {
  if(!address || address>0xffffff) return {};
  char key[11];snprintf(key,sizeof(key),"_%lu",static_cast<unsigned long>(address));
  nvs_handle_t handle;
  if(nvs_open("ShadeCodes",NVS_READWRITE,&handle)!=ESP_OK) return {};
  // Once a set was attempted, conservatively report uncertainty for any
  // failure. Never write the previous value as a supposed rollback.
  bool saved=nvs_set_u16(handle,key,code)==ESP_OK;
  if(saved) saved=nvs_commit(handle)==ESP_OK;
  uint16_t readback=0;
  if(saved) saved=nvs_get_u16(handle,key,&readback)==ESP_OK && readback==code;
  nvs_close(handle);
  return {saved?CounterEditState::saved:CounterEditState::uncertain};
}

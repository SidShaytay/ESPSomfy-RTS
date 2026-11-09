#include "CounterLoad.h"
#include "ModelLoadSnapshot.h"
#include "RollingCode.h"
#include <nvs.h>
#include <stdio.h>
#include <string.h>
namespace {
struct Handle {
  nvs_handle_t value=0;
  bool opened=false;
  ~Handle(){if(opened)nvs_close(value);}
  esp_err_t open(bool readOnly){const auto error=nvs_open("ShadeCodes",readOnly?NVS_READONLY:NVS_READWRITE,&value);opened=error==ESP_OK;return error;}
};
void key(uint32_t address,char (&text)[11]){snprintf(text,sizeof(text),"_%lu",static_cast<unsigned long>(address));}
bool read(nvs_handle_t handle,uint32_t address,uint16_t &value){
  char text[11];key(address,text);uint16_t stored=0;
  const auto error=nvs_get_u16(handle,text,&stored);
  if(error!=ESP_OK && error!=ESP_ERR_NVS_NOT_FOUND)return false;
  value=error==ESP_OK?stored:0;return true;
}
}
CounterLoad::CounterLoad():CounterLoad(modelLoadIo().allocate,modelLoadIo().release){}
CounterLoad::CounterLoad(void *(*allocate)(size_t),void (*release)(void *,size_t)):
  allocateBytes(allocate),releaseBytes(release){}
void CounterLoad::clear(){
  if(entries){volatile uint8_t *bytes=reinterpret_cast<volatile uint8_t *>(entries);for(size_t i=0;i<maximumEntries*sizeof(Entry);++i)bytes[i]=0;releaseBytes(entries,maximumEntries*sizeof(Entry));}
  entries=nullptr;count=0;outcome={};
}
CounterLoadResult CounterLoad::fail(CounterLoadStage stage,bool written){
  written|=outcome.writeAttempted;
  clear();outcome={written?CounterLoadState::uncertain:CounterLoadState::failed,stage,written};return outcome;
}
bool CounterLoad::include(uint32_t address,uint16_t floor){
  if(outcome.state!=CounterLoadState::collecting){if(outcome.ready())fail(CounterLoadStage::state);return false;}
  if(!address)return true;
  if(address>0xffffff){fail(CounterLoadStage::address);return false;}
  for(size_t i=0;i<count;++i)if(entries[i].address==address){entries[i].maximum=rollingCodeAfterLoad(entries[i].maximum,floor);return true;}
  if(count==maximumEntries){fail(CounterLoadStage::capacity);return false;}
  if(!entries){entries=static_cast<Entry *>(allocateBytes(maximumEntries*sizeof(Entry)));if(!entries){fail(CounterLoadStage::memory);return false;}memset(entries,0,maximumEntries*sizeof(Entry));}
  entries[count++]={address,floor,0};return true;
}
CounterLoadResult CounterLoad::resolve(CounterLoadControl control){
  if(outcome.state!=CounterLoadState::collecting)return outcome;
  bool promote=false,written=false;
  auto continuing=[&](){
    if(control.cooperate)control.cooperate(control.context);
    if(control.continuing && !control.continuing(control.context)){fail(CounterLoadStage::interrupted,written);return false;}
    return true;
  };
  if(!continuing())return outcome;
  {
    Handle handle;const auto error=count?handle.open(true):ESP_ERR_NVS_NOT_FOUND;
    if(error!=ESP_OK && error!=ESP_ERR_NVS_NOT_FOUND)return fail(CounterLoadStage::readOpen);
    for(size_t i=0;i<count;++i){
      if(!continuing())return outcome;
      if(error==ESP_OK && !read(handle.value,entries[i].address,entries[i].stored))return fail(CounterLoadStage::readValue);
      entries[i].maximum=rollingCodeAfterLoad(entries[i].stored,entries[i].maximum);
      promote|=entries[i].maximum>entries[i].stored;
    }
  }
  if(promote){
    {
      if(!continuing())return outcome;
      Handle handle;if(handle.open(false)!=ESP_OK)return fail(CounterLoadStage::writeOpen);
      for(size_t i=0;i<count;++i){
        if(entries[i].maximum<=entries[i].stored)continue;
        if(!continuing())return outcome;
        char text[11];key(entries[i].address,text);
        written=true;
        if(nvs_set_u16(handle.value,text,entries[i].maximum)!=ESP_OK)return fail(CounterLoadStage::writeValue,true);
      }
      if(!continuing())return outcome;
      if(nvs_commit(handle.value)!=ESP_OK)return fail(CounterLoadStage::commit,true);
    }
    // Verify through a NEW read-only handle after the write handle closed.
    if(!continuing())return outcome;
    Handle handle;if(handle.open(true)!=ESP_OK)return fail(CounterLoadStage::verifyOpen,true);
    for(size_t i=0;i<count;++i){if(!continuing())return outcome;uint16_t actual=0;if(!read(handle.value,entries[i].address,actual)||actual!=entries[i].maximum)return fail(CounterLoadStage::verifyValue,true);}
  }
  if(!continuing())return outcome;
  outcome={CounterLoadState::ready,CounterLoadStage::none,written};return outcome;
}
bool CounterLoad::lookup(uint32_t address,uint16_t &value)const{
  if(!outcome.ready()||!address)return false;
  for(size_t i=0;i<count;++i)if(entries[i].address==address){value=entries[i].maximum;return true;}
  return false;
}

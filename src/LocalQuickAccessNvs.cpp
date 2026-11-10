#include "LocalQuickAccessNvs.h"
#include <nvs.h>
#include <string.h>
namespace LocalUi {
namespace {constexpr const char *space="LCDUI",*key="quickV1";}
QuickAccessLoad readQuickAccessPreferences(QuickAccess &out) {
 out={};nvs_handle_t handle;
 auto status=nvs_open(space,NVS_READONLY,&handle);
 if(status==ESP_ERR_NVS_NOT_FOUND)return QuickAccessLoad::missing;
 if(status!=ESP_OK)return QuickAccessLoad::unreadable;
 size_t size=0;status=nvs_get_blob(handle,key,nullptr,&size);
 if(status!=ESP_OK || size!=quickAccessBytes) {
  nvs_close(handle);
  return status==ESP_ERR_NVS_NOT_FOUND?QuickAccessLoad::missing:
    status==ESP_ERR_NVS_TYPE_MISMATCH || status==ESP_OK?QuickAccessLoad::invalid:QuickAccessLoad::unreadable;
 }
 uint8_t bytes[quickAccessBytes];size=sizeof(bytes);status=nvs_get_blob(handle,key,bytes,&size);nvs_close(handle);
 if(status!=ESP_OK)return QuickAccessLoad::unreadable;
 return decodeQuickAccess(bytes,size,out);
}
QuickPreferenceSave writeQuickAccessPreferences(const QuickAccess &value,QuickPreferenceWitness &witness) {
 uint8_t bytes[quickAccessBytes];
 if(!witness.current() || !encodeQuickAccess(value,bytes,sizeof(bytes)))return QuickPreferenceSave::failed;
 nvs_handle_t handle;if(nvs_open(space,NVS_READWRITE,&handle)!=ESP_OK)return QuickPreferenceSave::failed;
 if(!witness.current()){nvs_close(handle);return QuickPreferenceSave::failed;}
 const auto written=nvs_set_blob(handle,key,bytes,sizeof(bytes));
 if(written!=ESP_OK || !witness.current()){nvs_close(handle);return QuickPreferenceSave::unknown;}
 const auto committed=nvs_commit(handle);nvs_close(handle);
 if(committed!=ESP_OK || !witness.current())return QuickPreferenceSave::unknown;
 QuickAccess actual;uint8_t verified[quickAccessBytes];
 if(readQuickAccessPreferences(actual)!=QuickAccessLoad::valid || !encodeQuickAccess(actual,verified,sizeof(verified)) ||
    memcmp(bytes,verified,sizeof(bytes)) || !witness.current())return QuickPreferenceSave::unknown;
 return QuickPreferenceSave::saved;
}
}

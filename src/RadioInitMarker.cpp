#include "RadioInitMarker.h"
#include "SettingsDurability.h"
#include <nvs.h>
settings_save_result_t writeRadioInitMarker(bool ready) {
  nvs_handle_t handle=0;
  if(nvs_open("CC1101",NVS_READWRITE,&handle)!=ESP_OK)
    return {settings_save_state_t::failed,"radioInit"};
  const uint8_t value=ready?1:0;uint8_t readback=2;
  bool saved=nvs_set_u8(handle,"radioInit",value)==ESP_OK;
  if(saved)saved=nvs_commit(handle)==ESP_OK;
  if(saved)saved=nvs_get_u8(handle,"radioInit",&readback)==ESP_OK && readback==value;
  nvs_close(handle);
  if(!saved) {
    // A set was attempted; never claim the earlier marker was restored.
    settingsDurability().markUncertain(settingsNamespaceBit(SettingsNamespace::radio));
    return {settings_save_state_t::partial,"radioInit"};
  }
  return {}; // Marker success cannot reconcile an uncertain full namespace.
}

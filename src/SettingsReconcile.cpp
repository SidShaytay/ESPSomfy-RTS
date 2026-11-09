#include "SettingsReconcile.h"
#include "Transceiver.h"
namespace {
uint8_t affectedSections(SettingsNamespace space) {
  switch(space) {
    case SettingsNamespace::cfg: return (1u<<static_cast<uint8_t>(settings_section_t::general))|(1u<<static_cast<uint8_t>(settings_section_t::network));
    case SettingsNamespace::ntp: return 1u<<static_cast<uint8_t>(settings_section_t::general);
    case SettingsNamespace::wifi:
    case SettingsNamespace::ethernet:
    case SettingsNamespace::ip: return 1u<<static_cast<uint8_t>(settings_section_t::network);
    case SettingsNamespace::mqtt: return 1u<<static_cast<uint8_t>(settings_section_t::mqtt);
    case SettingsNamespace::security: return 1u<<static_cast<uint8_t>(settings_section_t::security);
    case SettingsNamespace::radio: return 1u<<static_cast<uint8_t>(settings_section_t::radio);
  }
  return 0;
}
}
settings_reconcile_result_t reconcileRunningSettings(ConfigSettings &current,const transceiver_config_t &radio,SettingsEffects &effects) {
  settings_reconcile_result_t out;
  out.requested=settingsDurability().status().uncertain;
  if(!current.Security.loadTrusted()) {
    out.persistence={settings_save_state_t::failed,"SEC load"};
    out.durability=settingsDurability().status();
    return out;
  }
  uint8_t completed=0;
  bool partial=false;
  for(uint8_t n=0;n<8;n++) {
    const auto space=static_cast<SettingsNamespace>(n);
    const uint8_t bit=settingsNamespaceBit(space);
    if(!(out.requested&bit)) continue;
    settings_save_result_t result;
    switch(space) {
      case SettingsNamespace::cfg: result=current.save();break;
      case SettingsNamespace::ntp: result=current.NTP.save();break;
      case SettingsNamespace::wifi: result=current.WIFI.save();break;
      case SettingsNamespace::ethernet:
#if HAS_ETHERNET
        result=current.Ethernet.save();
#else
        result={settings_save_state_t::failed,"ETH"};
#endif
        break;
      case SettingsNamespace::ip: result=current.IP.save();break;
      case SettingsNamespace::mqtt: result=current.MQTT.save();break;
      case SettingsNamespace::security: result=current.Security.save();break;
      case SettingsNamespace::radio: {
        // Only normalization on this copy; never setConfig/apply/reinitialize.
        transceiver_config_t copy=radio;
        result=copy.savePreservingInit();break;
      }
    }
    partial|=result.state==settings_save_state_t::partial;
    if(result.saved()) {
      out.reconciled|=bit;completed|=affectedSections(space);
    } else if(out.persistence.saved()) out.persistence=result;
  }
  if(out.persistence.saved() && (!current.settingsUsable() || !radio.loadStatus.usable()))
    out.persistence={settings_save_state_t::failed,"settings load"};
  if(!out.persistence.saved() && (out.reconciled || partial)) out.persistence.state=settings_save_state_t::partial;
  out.durability=settingsDurability().status();
  // Notify only after every attempted writer has updated the ledger. A saved
  // namespace contains current live values even when another reconcile fails.
  for(uint8_t section=0;section<5;section++)
    if(completed&(1u<<section)) effects.completed(static_cast<settings_section_t>(section),false);
  return out;
}

#pragma once
#include "ConfigSettings.h"
#include "ProtoSettings.h"
#include "Utils.h"
#include "LegacyRestartSource.h"

struct settings_completion_t {
  settings_section_t section;
  security_types securityBefore, securityAfter;
  uint32_t originSocketClient = 0; // zero means a local save, so no client is excluded
  bool reboot = false;
};
// loop() only, after a successful complete save. ClientNotifications is the
// ProtoSocket interface: closeAll(origin) closes other clients now and the
// origin after its queued reply drains; settingsChanged excludes the origin.
// Socket callers queue their reply before returning to ProtoSocket::loop().
template<class ClientNotifications>
RestartResult completeSettingsSave(const settings_completion_t &saved, ClientNotifications &clients, rebootDelay_t &restart, uint32_t now, RestartResult admission = {true,RestartReason::none}) {
  const bool reauthenticate = saved.section == settings_section_t::security &&
    (saved.securityBefore != saved.securityAfter || saved.securityAfter != security_types::None);
  if(reauthenticate) clients.closeAll(saved.originSocketClient);
  else clients.settingsChanged(saved.originSocketClient,settingsSectionName(saved.section));
  if(!saved.reboot) return {};
  if(!admission.admitted) return admission;
  return publishGenericLegacyRestart(restart,now+1000);
}

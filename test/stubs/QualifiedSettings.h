#pragma once
#include "ConfigSettings.h"
#include "Transceiver.h"
// Synthetic host fixtures represent settings AFTER a qualified boot. Real
// unknown/failed/invalid loading is exercised by the namespace-load tests.
template<class T> void qualifySettings(T &value){value.loadStatus={SettingsLoadState::knownEmpty};}
inline void qualifySettings(SecuritySettings &value){value.loadState=SecurityLoadState::knownEmpty;}
inline void qualifySettings(ConfigSettings &value){
 value.loadStatus={SettingsLoadState::knownEmpty};if(value.connType==conn_types_t::unset)value.connType=conn_types_t::wifi;
 qualifySettings(value.NTP);qualifySettings(value.WIFI);qualifySettings(value.IP);qualifySettings(value.MQTT);qualifySettings(value.Security);
#if HAS_ETHERNET
 qualifySettings(value.Ethernet);
#endif
}

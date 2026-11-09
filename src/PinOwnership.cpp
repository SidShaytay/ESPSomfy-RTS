#if defined(ARDUINO) && defined(PIN_OWNERSHIP_HOST_TEST)
#error "Pin ownership host hooks are forbidden in firmware"
#endif
#include "PinOwnership.h"
#ifndef PIN_OWNERSHIP_HOST_TEST
#include "Somfy.h"
#include "ESPNetwork.h"
#include "SettingsTypedValidation.h"
extern SomfyShadeController somfy;
extern ConfigSettings settings;
extern ESPNetwork net;
#endif
#include "ShadeGpioPolicy.h"
namespace {
uint64_t modelGpioClaims() {
  uint64_t claims=0;
  for(auto &shade:somfy.shades)if(shade.getShadeId()!=255)
    claims|=ShadeGpioPolicy::pinClaims(shade.proto,shade.gpioUp,shade.gpioDown,shade.gpioMy);
  return claims|retainedGpioPinClaims();
}
}
uint64_t retainedEthernetPinClaims(){return net.retainedPinClaims();}
RadioPinContext withRetainedEthernetClaims(RadioPinContext context){context.ethernetClaims|=retainedEthernetPinClaims();return context;}
RadioPinContext currentRadioPinContext(){return radioPinContext(settings,retainedEthernetPinClaims());}
bool radioPinConfigurationAvailable(const transceiver_config_t &cfg,RadioPinContext context) {
  context=withRetainedEthernetClaims(context);
  if(!somfy.modelLoad.usable() || !context.qualified)return false;
  const uint64_t attempted=cfg.pinClaims()|somfy.transceiver.retainedPinClaims();
  return !(attempted&(modelGpioClaims()|context.ethernetClaims));
}
bool currentRadioPinConfigurationAvailable(const transceiver_config_t &cfg){return radioPinConfigurationAvailable(cfg,currentRadioPinContext());}
bool networkPinConfigurationAvailable(const ConfigSettings &cfg) {
#if HAS_ETHERNET
  if(!cfg.loadStatus.readable() || !validSettings(cfg))return false;
  if(cfg.connType!=conn_types_t::ethernet && cfg.connType!=conn_types_t::ethernetpref)return true;
  if(!somfy.modelLoad.usable() || !cfg.Ethernet.loadStatus.readable() || !validSettings(cfg.Ethernet) ||
      !somfy.transceiver.config.loadStatus.usable() || !validSettings(somfy.transceiver.config))return false;
  const uint64_t claimed=cfg.Ethernet.pinClaims()|retainedEthernetPinClaims();
  return !(claimed&(modelGpioClaims()|somfy.transceiver.config.pinClaims()|somfy.transceiver.retainedPinClaims()));
#else
  (void)cfg;return true;
#endif
}
bool ethernetActivationPinsAvailable(const ConfigSettings &cfg) {
#if HAS_ETHERNET
  return (cfg.connType==conn_types_t::ethernet || cfg.connType==conn_types_t::ethernetpref) && networkPinConfigurationAvailable(cfg);
#else
  (void)cfg;return false;
#endif
}

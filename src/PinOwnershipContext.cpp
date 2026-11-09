#include "PinOwnership.h"
#include "ConfigSettings.h"
#include "Transceiver.h"
#include "SettingsTypedValidation.h"
RadioPinContext radioPinContext(const ConfigSettings &cfg,uint64_t retainedEthernet) {
  RadioPinContext context;context.ethernetClaims=retainedEthernet;
#if HAS_ETHERNET
  if(!cfg.loadStatus.readable() || !validSettings(cfg))return context;
  if(cfg.connType==conn_types_t::ethernet || cfg.connType==conn_types_t::ethernetpref) {
    if(!cfg.Ethernet.loadStatus.readable() || !validSettings(cfg.Ethernet))return context;
    context.ethernetClaims|=cfg.Ethernet.pinClaims();
  }
#else
  (void)cfg;
#endif
  context.qualified=true;return context;
}

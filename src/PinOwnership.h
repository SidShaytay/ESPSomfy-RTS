#pragma once
#include <stdint.h>
class ConfigSettings;
struct transceiver_config_t;
// A qualified projected other-bus map, never credentials or a new owner.
// Protected radio admission always adds the fresh retained ETH map itself.
struct RadioPinContext {uint64_t ethernetClaims=0;bool qualified=false;};
static_assert(sizeof(RadioPinContext)<=16,"Bounded projected radio pin context");
RadioPinContext radioPinContext(const ConfigSettings &,uint64_t retainedEthernet);
RadioPinContext currentRadioPinContext();
RadioPinContext withRetainedEthernetClaims(RadioPinContext);
uint64_t retainedEthernetPinClaims();
bool radioPinConfigurationAvailable(const transceiver_config_t &,RadioPinContext);
bool currentRadioPinConfigurationAvailable(const transceiver_config_t &);
bool networkPinConfigurationAvailable(const ConfigSettings &);
bool ethernetActivationPinsAvailable(const ConfigSettings &);

#pragma once
#include "ConfigSettings.h"
#include "ConfigRecords.h"
#include "RestoreResult.h"
#include "SettingsApply.h"
#include "ModelEditor.h"
#include "GitUpdateOwner.h"
#include "RadioUpdateActivity.h"
#include "PinOwnership.h"
#include <stddef.h>
class Transceiver;
struct transceiver_config_t;
// Private bounded typed drafts. No settings/secret accessor and no writes/SDK effects in prepare.
class SelectedSettingsRestore {
  struct Draft;
  Draft *draft=nullptr;
  RestoreSettingsResult outcome;
  bool prepared=false;
  void *(*allocateBytes)(size_t);
  void (*releaseBytes)(void *,size_t);
  void clear();
public:
  static constexpr size_t maximumDraftBytes=2048;
  SelectedSettingsRestore();
  SelectedSettingsRestore(void *(*allocate)(size_t),void (*release)(void *,size_t));
  ~SelectedSettingsRestore();
  SelectedSettingsRestore(const SelectedSettingsRestore &)=delete;
  SelectedSettingsRestore &operator=(const SelectedSettingsRestore &)=delete;
  // reader begins at the settings/net/radio tail of the immutable snapshot.
  // Checked decoding is scoped here; ordinary legacy read mode is restored.
  bool prepare(const ConfigSettings &,const transceiver_config_t &,const restore_options_t &,
      const config_header_t &,ConfigReader &,bool sameBoard,uint64_t retainedRadio,uint64_t retainedEthernet);
  int8_t validateGPIOs(radio_proto,uint8_t up,uint8_t down,uint8_t my,
      const ConfigSettings &,const transceiver_config_t &,
      const pin_users_t &otherPins,const pin_users_t &invalidOutputs,
      uint64_t retainedRadio,uint64_t retainedEthernet)const;
  const RestoreSettingsResult &status()const{return outcome;}
  // Existing claimed restore owner + exact pause, no nested acquisition.
  // Only all-success permits non-radio RAM adoption/runtime effects. A returned
  // applied MQTT bit also certifies mqttChanged() returned (no eager reconnect).
  RestoreSettingsResult apply(ConfigSettings &,Transceiver &,const GitUpdateOwnerToken &,
      const RadioInstallToken &,SettingsEffects &);
};

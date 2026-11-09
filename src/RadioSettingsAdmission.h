#pragma once
#include "SettingsSaveResult.h"
class Transceiver;
struct transceiver_config_t;
settings_save_result_t saveRadioConfiguration(Transceiver &,const transceiver_config_t &);
// Loop-owned reducing cleanup; retains the exact owner/seals if resume is busy.
void serviceRadioSettingsCleanup();

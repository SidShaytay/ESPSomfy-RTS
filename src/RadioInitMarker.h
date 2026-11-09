#pragma once
#include "SettingsSaveResult.h"
// Updates only the operational safety marker; never certifies the complete
// CC1101 namespace or clears an earlier uncertain full-settings write.
settings_save_result_t writeRadioInitMarker(bool ready);

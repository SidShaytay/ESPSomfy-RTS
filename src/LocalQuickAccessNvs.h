#pragma once
#include "LocalQuickAccess.h"
namespace LocalUi {
enum class QuickPreferenceSave:uint8_t {saved,failed,unknown};
class QuickPreferenceWitness {public:virtual ~QuickPreferenceWitness()=default;virtual bool current()=0;};
QuickAccessLoad readQuickAccessPreferences(QuickAccess &);
// Caller holds the configuration lease through write, commit and fresh readback.
QuickPreferenceSave writeQuickAccessPreferences(const QuickAccess &,QuickPreferenceWitness &);
}

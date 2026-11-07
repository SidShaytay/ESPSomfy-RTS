#pragma once
#include "LocalUi.h"
namespace LocalUi {
Model snapshotModel(bool running);
DisplayPreferences loadPreferences();
bool savePreferences(const DisplayPreferences &);
bool dispatch(const Command &);
} // namespace LocalUi

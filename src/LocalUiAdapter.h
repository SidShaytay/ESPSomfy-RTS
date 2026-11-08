#pragma once
#include "LocalUi.h"
namespace LocalUi {
Model snapshotModel(bool running);
void focusAuthorizationTarget(uint8_t selected);
DisplayPreferences loadPreferences();
bool savePreferences(const DisplayPreferences &);
bool dispatch(const Command &);
} // namespace LocalUi

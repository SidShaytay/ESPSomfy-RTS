#pragma once
#include "ModelCommitResult.h"
#include "SettingsSaveResult.h"
#include <stdint.h>
// Facts from this restore only. Never infer application or durability from a
// controller's earlier lastModelCommit, and never put backup values in results.
enum class RestoreRefusal : uint8_t { none, ownership, busy, invalid, incompleteModel, counterStorage, modelStorage, settings };
enum class RestoreSettingsState : uint8_t { notSelected, prepared, unavailable, refused, failed, partial, saved };
struct RestoreSettingsResult {
  RestoreSettingsState state=RestoreSettingsState::notSelected;
  settings_save_result_t persistence;
  // SettingsNamespace bits, not UI sections (general/connection alias CFG).
  uint8_t requestedNamespaces=0,verifiedNamespaces=0,appliedNamespaces=0;
  bool saved()const {
    return state==RestoreSettingsState::notSelected ? requestedNamespaces==0 :
      state==RestoreSettingsState::saved && requestedNamespaces==verifiedNamespaces &&
      requestedNamespaces==appliedNamespaces;
  }
};
struct RestoreApplyResult {
  bool complete=false,modelApplied=false,counterWriteAttempted=false;
  RestoreRefusal refusal=RestoreRefusal::invalid;
  ModelCommitResult modelFile;
  RestoreSettingsResult settings;
  bool partial()const {
    return !complete && (modelApplied || counterWriteAttempted ||
      settings.verifiedNamespaces || settings.appliedNamespaces ||
      settings.persistence.state==settings_save_state_t::partial);
  }
};

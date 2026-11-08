#include <unity.h>
#include "SettingsDurability.h"
#include "ProtoSettings.h"
void setUp() {}
void tearDown() {}
static void namespaces_clear_independently_only_after_full_verified_save() {
  SettingsDurability ledger;ledger.markUncertain(255);
  const auto epoch=ledger.status().epoch;
  for(uint8_t n=0;n<8;n++) {
    const auto space=static_cast<SettingsNamespace>(n);
    ledger.fullSave(space,{settings_save_state_t::failed,"key"});
    TEST_ASSERT_TRUE(ledger.status().uncertain&settingsNamespaceBit(space));
    ledger.fullSave(space,{});
    TEST_ASSERT_FALSE(ledger.status().uncertain&settingsNamespaceBit(space));
    TEST_ASSERT_EQUAL_UINT64(epoch,ledger.status().epoch);
  }
  TEST_ASSERT_TRUE(ledger.status().restartAllowed());
}
static void cfg_alias_and_composite_masks_use_persisted_namespaces() {
  TEST_ASSERT_EQUAL(settingsNamespace(settings_part_t::general),settingsNamespace(settings_part_t::conn));
  TEST_ASSERT_EQUAL_UINT8(settingsNamespaceBit(SettingsNamespace::cfg),settingsNamespaces((1u<<0)|(1u<<2)));
  TEST_ASSERT_EQUAL_UINT8(255,settingsNamespaces(511));
}
static void repeated_uncertainty_invalidates_epochs_and_reconcile_never_rewinds() {
  SettingsDurability ledger;
  const auto start=ledger.status().epoch;
  ledger.markUncertain(settingsNamespaceBit(SettingsNamespace::security));
  const auto first=ledger.status().epoch;
  ledger.markUncertain(settingsNamespaceBit(SettingsNamespace::security));
  TEST_ASSERT_GREATER_THAN_UINT64(start,first);
  TEST_ASSERT_GREATER_THAN_UINT64(first,ledger.status().epoch);
  const auto last=ledger.status().epoch;
  ledger.fullSave(SettingsNamespace::security,{});
  TEST_ASSERT_TRUE(ledger.status().restartAllowed());TEST_ASSERT_EQUAL_UINT64(last,ledger.status().epoch);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(namespaces_clear_independently_only_after_full_verified_save);
  RUN_TEST(cfg_alias_and_composite_masks_use_persisted_namespaces);
  RUN_TEST(repeated_uncertainty_invalidates_epochs_and_reconcile_never_rewinds);
  return UNITY_END();
}

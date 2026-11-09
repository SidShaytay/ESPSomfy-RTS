#include "RestoreResult.h"
#include <unity.h>
void setUp(){}void tearDown(){}
static void selected_namespaces_require_complete_verified_live_adoption(){
 RestoreSettingsResult result;TEST_ASSERT_TRUE(result.saved());result.requestedNamespaces=1;TEST_ASSERT_FALSE(result.saved());
 result.state=RestoreSettingsState::saved;result.verifiedNamespaces=1;TEST_ASSERT_FALSE(result.saved());
 result.appliedNamespaces=1;TEST_ASSERT_TRUE(result.saved());result.state=RestoreSettingsState::partial;TEST_ASSERT_FALSE(result.saved());
 result.state=RestoreSettingsState::saved;result.verifiedNamespaces=3;TEST_ASSERT_FALSE(result.saved());
}
static void default_never_claims_completion_and_side_effects_are_explicit(){
 RestoreApplyResult result;TEST_ASSERT_FALSE(result.complete);TEST_ASSERT_FALSE(result.partial());
 result.counterWriteAttempted=true;TEST_ASSERT_TRUE(result.partial());result.counterWriteAttempted=false;result.modelApplied=true;TEST_ASSERT_TRUE(result.partial());
 result.modelApplied=false;result.settings.verifiedNamespaces=1;TEST_ASSERT_TRUE(result.partial());result.complete=true;TEST_ASSERT_FALSE(result.partial());
}
int main(){UNITY_BEGIN();RUN_TEST(selected_namespaces_require_complete_verified_live_adoption);RUN_TEST(default_never_claims_completion_and_side_effects_are_explicit);return UNITY_END();}

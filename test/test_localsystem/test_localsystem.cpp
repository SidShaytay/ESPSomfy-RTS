#include <unity.h>
#include "LocalSystem.h"
#include <string.h>
using namespace LocalUi;
struct State : System::Backend {
  SecuritySnapshot auth{AuthMode::none,true,false,1,1};
  RestartSnapshot current;
  RestartResult result{true,RestartReason::none};
  unsigned restarts=0;bool immediateCancel=false,zeroGeneration=false;
  SecuritySnapshot authorization() override {return auth;}
  void activity() override {}
  RestartResult restart(uint32_t session,uint32_t configuration) override {TEST_ASSERT_EQUAL(auth.generation,session);TEST_ASSERT_EQUAL(auth.configurationGeneration,configuration);++restarts;if(result.admitted){current.generation=zeroGeneration?0:1;current.phase=immediateCancel?RestartPhase::cancelled:RestartPhase::admitted;current.reason=immediateCancel?RestartReason::cancelled:RestartReason::none;}return result;}
  RestartSnapshot status() override {return current;}
};
void setUp(){} void tearDown(){}
static void navigation_and_cancel_never_restart() {
  State state;System::Form form(state);EntityAdmin::FormView view;
  TEST_ASSERT_TRUE(form.open(1));form.render(view);
  TEST_ASSERT_EQUAL_STRING("System",view.title);TEST_ASSERT_EQUAL(2,view.total);
  form.activate(2);form.render(view);TEST_ASSERT_EQUAL_STRING("Cancel",view.rows[0].label);
  form.activate(3);TEST_ASSERT_EQUAL(0,state.restarts); // default Cancel
  form.turn(1,4);form.activate(5);form.render(view);
  TEST_ASSERT_EQUAL_STRING("Browser files",view.title);TEST_ASSERT_EQUAL(4,view.total);
  form.activate(6);form.back(7);form.back(8);form.back(9);
  TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_EQUAL(0,state.restarts);
}
static void explicit_restart_is_once_and_tracks_its_result() {
  State state;System::Form form(state);EntityAdmin::FormView view;form.open(1);
  form.activate(2);form.turn(1,3);form.activate(4);
  TEST_ASSERT_EQUAL(1,state.restarts);form.render(view);
  TEST_ASSERT_EQUAL_STRING("Restart scheduled",view.title);
  form.activate(5);TEST_ASSERT_EQUAL(1,state.restarts);
  state.current.phase=RestartPhase::cancelled;state.current.reason=RestartReason::queuedOperations;
  form.tick(6);form.render(view);TEST_ASSERT_EQUAL_STRING("Restart stopped",view.title);
  TEST_ASSERT_NOT_NULL(strstr(view.hint,"queued"));
}
static void auth_change_expiry_and_backward_clock_discard_confirmation() {
  for(unsigned reason=0;reason<4;++reason) {
    State state;System::Form form(state);form.open(100);form.activate(101);form.turn(1,102);
    if(reason==0)state.auth.allowed=false;
    if(reason==1)++state.auth.generation;
    if(reason==2)++state.auth.configurationGeneration;
    form.activate(reason==3?99:103);
    TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_EQUAL(0,state.restarts);
  }
  State state;System::Form form(state);form.open(1);form.activate(2);form.turn(1,3);form.activate(120003);
  TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_EQUAL(0,state.restarts);
}
static void refusal_does_not_claim_saved_or_replay() {
  State state;state.result={false,RestartReason::modelUncertain};System::Form form(state);EntityAdmin::FormView view;
  form.open(1);form.activate(2);form.turn(1,3);form.activate(4);form.render(view);
  TEST_ASSERT_EQUAL_STRING("Restart refused",view.title);TEST_ASSERT_EQUAL(EntityAdmin::Notice::none,view.notice);
  TEST_ASSERT_NOT_NULL(strstr(view.hint,"uncertain"));form.activate(5);TEST_ASSERT_EQUAL(1,state.restarts);
}
static void unreadable_model_requires_verified_restore_guidance() {
  State state;state.result={false,RestartReason::modelUnavailable};
  System::Form form(state);EntityAdmin::FormView view;
  form.open(1);form.activate(2);form.turn(1,3);form.activate(4);form.render(view);
  TEST_ASSERT_EQUAL_STRING("Restart refused",view.title);
  TEST_ASSERT_EQUAL_STRING("Model not loaded. Use a verified full model restore.",view.hint);
  form.activate(5);TEST_ASSERT_EQUAL(1,state.restarts);
}
static void unreadable_security_never_suggests_credentials_were_reset() {
  State state;state.result={false,RestartReason::securityUnavailable};
  System::Form form(state);EntityAdmin::FormView view;
  form.open(1);form.activate(2);form.turn(1,3);form.activate(4);form.render(view);
  TEST_ASSERT_EQUAL_STRING("Restart refused",view.title);
  TEST_ASSERT_EQUAL_STRING("Security not loaded. Authentication is unavailable.",view.hint);
  form.activate(5);TEST_ASSERT_EQUAL(1,state.restarts);
}
static void immediate_cancel_and_invalid_binding_never_claim_scheduled() {
  for(unsigned invalid=0;invalid<2;++invalid) {
    State state;state.immediateCancel=!invalid;state.zeroGeneration=invalid;
    System::Form form(state);EntityAdmin::FormView view;form.open(1);form.activate(2);form.turn(1,3);form.activate(4);form.render(view);
    TEST_ASSERT_EQUAL_STRING(invalid?"Restart status unknown":"Restart stopped",view.title);
    TEST_ASSERT_EQUAL(1,state.restarts);
  }
}
static void superseded_generation_never_borrows_another_result() {
  State state;System::Form form(state);EntityAdmin::FormView view;form.open(1);form.activate(2);form.turn(1,3);form.activate(4);
  state.current.generation=2;state.current.phase=RestartPhase::cancelled;state.current.reason=RestartReason::modelUncertain;
  form.tick(5);form.render(view);TEST_ASSERT_EQUAL_STRING("Restart stopped",view.title);
  TEST_ASSERT_NOT_NULL(strstr(view.hint,"State changed"));TEST_ASSERT_NULL(strstr(view.hint,"uncertain"));
  form.activate(6);TEST_ASSERT_EQUAL(1,state.restarts);
}
int main(int,char**) {UNITY_BEGIN();RUN_TEST(unreadable_security_never_suggests_credentials_were_reset);RUN_TEST(unreadable_model_requires_verified_restore_guidance);RUN_TEST(immediate_cancel_and_invalid_binding_never_claim_scheduled);RUN_TEST(superseded_generation_never_borrows_another_result);RUN_TEST(navigation_and_cancel_never_restart);RUN_TEST(explicit_restart_is_once_and_tracks_its_result);RUN_TEST(auth_change_expiry_and_backward_clock_discard_confirmation);RUN_TEST(refusal_does_not_claim_saved_or_replay);return UNITY_END();}

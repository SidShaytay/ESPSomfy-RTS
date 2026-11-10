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
  TEST_ASSERT_EQUAL_STRING("System",view.title);TEST_ASSERT_EQUAL(5,view.total);
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
static void child_input_refreshes_inactivity_but_completion_does_not() {
 State state;System::Form f(state);f.open(0);f.turn(1,1);f.turn(1,2);f.activate(3);
 TEST_ASSERT_TRUE(f.wantsWifiScan());f.childActivity(119999);f.tick(120001);TEST_ASSERT_FALSE(f.closed());
 f.tick(240000);TEST_ASSERT_TRUE(f.closed());TEST_ASSERT_EQUAL(0,state.restarts);
}
static void untrusted_settings_have_distinct_repair_message_without_replay() {
  State state;state.result={false,RestartReason::settingsUnavailable};
  System::Form form(state);EntityAdmin::FormView view;
  form.open(1);form.activate(2);form.turn(1,3);form.activate(4);form.render(view);
  TEST_ASSERT_EQUAL_STRING("Restart refused",view.title);
  TEST_ASSERT_EQUAL_STRING("Settings need repair. Review affected section.",view.hint);
  form.activate(5);TEST_ASSERT_EQUAL(1,state.restarts);
}
static void diagnostics_is_explicit_and_returns_to_its_system_row() {
 State b;System::Form f(b);EntityAdmin::FormView view;f.open(0);
 for(unsigned i=0;i<3;++i)f.turn(1,1);
 f.render(view);TEST_ASSERT_EQUAL(5,view.total);TEST_ASSERT_EQUAL_STRING("Receive diagnostics",view.rows[0].label);
 f.activate(2);TEST_ASSERT_TRUE(f.wantsDiagnostics());TEST_ASSERT_EQUAL(0,b.restarts);
 f.finishChild();f.render(view);TEST_ASSERT_EQUAL(3,view.selected);TEST_ASSERT_FALSE(f.wantsDiagnostics());
}
static void radio_scan_is_an_explicit_child_and_returns_to_its_row(){
 State backend;System::Form form(backend);EntityAdmin::FormView view;TEST_ASSERT_TRUE(form.open(0));
 for(unsigned i=0;i<4;++i)form.turn(1,1);form.render(view);TEST_ASSERT_EQUAL_STRING("Radio frequency scan",view.rows[1].label);
 form.activate(2);TEST_ASSERT_TRUE(form.wantsRadioScan());TEST_ASSERT_EQUAL(0,backend.restarts);
 form.finishChild();form.render(view);TEST_ASSERT_EQUAL(4,view.selected);form.back(3);TEST_ASSERT_TRUE(form.closed());
}
int main(int,char**) {UNITY_BEGIN();RUN_TEST(diagnostics_is_explicit_and_returns_to_its_system_row);RUN_TEST(untrusted_settings_have_distinct_repair_message_without_replay);RUN_TEST(child_input_refreshes_inactivity_but_completion_does_not);RUN_TEST(unreadable_security_never_suggests_credentials_were_reset);RUN_TEST(unreadable_model_requires_verified_restore_guidance);RUN_TEST(immediate_cancel_and_invalid_binding_never_claim_scheduled);RUN_TEST(superseded_generation_never_borrows_another_result);RUN_TEST(navigation_and_cancel_never_restart);RUN_TEST(explicit_restart_is_once_and_tracks_its_result);RUN_TEST(auth_change_expiry_and_backward_clock_discard_confirmation);RUN_TEST(refusal_does_not_claim_saved_or_replay);return UNITY_END();}


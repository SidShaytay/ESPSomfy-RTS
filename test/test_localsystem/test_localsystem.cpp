#include <unity.h>
#include "LocalSystem.h"
#include <string.h>
using namespace LocalUi;
struct State : System::Backend {
  SecuritySnapshot auth{AuthMode::none,true,false,1,1};
  RestartSnapshot current;
  RestartResult result{true,RestartReason::none};
  unsigned restarts=0;
  SecuritySnapshot authorization() override {return auth;}
  void activity() override {}
  RestartResult restart(uint32_t session,uint32_t configuration) override {TEST_ASSERT_EQUAL(auth.generation,session);TEST_ASSERT_EQUAL(auth.configurationGeneration,configuration);++restarts;if(result.admitted)current.phase=RestartPhase::admitted;return result;}
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
int main(int,char**) {UNITY_BEGIN();RUN_TEST(navigation_and_cancel_never_restart);RUN_TEST(explicit_restart_is_once_and_tracks_its_result);RUN_TEST(auth_change_expiry_and_backward_clock_discard_confirmation);RUN_TEST(refusal_does_not_claim_saved_or_replay);return UNITY_END();}

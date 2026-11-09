#include "LocalUpdates.h"
#include "LocalJsonAllocator.h"
#include <unity.h>
#include <cstdlib>
#include <cstring>
using namespace LocalUi;
struct Backend:Updates::Backend {
  SecuritySnapshot auth{AuthMode::pin,true,false,3,5};
  GitUpdateSnapshot value;
  GitUpdateBootStatus bootValue;
  unsigned checks=0,installs=0,cancels=0,activityCount=0;
  uint64_t cancelled=0,expectedCandidate=0;
  Backend(){value.supported=true;value.phase=GitUpdatePhase::idle;value.reason=GitUpdateReason::none;strcpy(value.installed,"v4.0.2");}
  SecuritySnapshot authorization()override{return auth;}
  void activity()override{++activityCount;}
  GitUpdateSnapshot status()override{return value;}
  GitUpdateBootStatus boot()override{return bootValue;}
  GitUpdateRequest check(uint32_t session,uint32_t security,uint64_t &operation)override{
    TEST_ASSERT_EQUAL(3,session);TEST_ASSERT_EQUAL(5,security);++checks;
    value.busy=value.cancellable=true;value.phase=GitUpdatePhase::checking;operation=value.operation=91;return GitUpdateRequest::accepted;
  }
  GitUpdateRequest install(uint64_t candidate,uint32_t session,uint32_t security,uint64_t &operation)override{
    TEST_ASSERT_EQUAL(3,session);TEST_ASSERT_EQUAL(5,security);++installs;expectedCandidate=candidate;
    if(candidate!=value.candidate.generation)return GitUpdateRequest::invalidCandidate;
    operation=value.operation=93;value.busy=value.cancellable=true;value.canInstall=false;value.phase=GitUpdatePhase::downloading;return GitUpdateRequest::accepted;
  }
  bool cancel(uint64_t operation,uint32_t,uint32_t)override{++cancels;cancelled=operation;return operation==value.operation && value.cancellable;}
  void available(){value.busy=false;value.cancellable=false;value.phase=GitUpdatePhase::available;value.canInstall=true;value.candidate.generation=92;strcpy(value.candidate.version,"v4294967295.4294967295.4294967295");}
};
void setUp(){}void tearDown(){}
static void check_is_explicit_and_progress_does_not_extend_authority(){
  Backend b;JsonAllocator pool(4096,malloc,free);Updates::Form form(b,pool);EntityAdmin::FormView view;
  TEST_ASSERT_TRUE(form.open(0));form.render(view);TEST_ASSERT_EQUAL(0,b.checks);TEST_ASSERT_TRUE(view.protectedFrame);
  form.activate(1);TEST_ASSERT_EQUAL(1,b.checks);const auto count=b.activityCount;
  form.tick(500);form.tick(1000);TEST_ASSERT_EQUAL(count,b.activityCount);
  b.available();form.tick(1001);form.render(view);TEST_ASSERT_NOT_NULL(strstr(view.rows[2].value,"4294967295"));
  form.back(1002);TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_EQUAL(0,b.cancels);TEST_ASSERT_EQUAL(0,pool.used());
}
static void confirmation_defaults_cancel_and_installs_exact_candidate_once(){
  Backend b;b.available();JsonAllocator pool(4096,malloc,free);Updates::Form form(b,pool);EntityAdmin::FormView view;
  form.open(0);form.turn(1,1);form.activate(2);form.render(view);
  TEST_ASSERT_EQUAL_STRING("Cancel",view.rows[0].label);TEST_ASSERT_EQUAL_STRING(b.value.candidate.version,view.rows[2].value);
  form.activate(3);TEST_ASSERT_EQUAL(0,b.installs);
  form.turn(1,4);form.activate(5);form.turn(1,6);form.activate(7);
  TEST_ASSERT_EQUAL(1,b.installs);TEST_ASSERT_EQUAL(92,b.expectedCandidate);
  b.value.cancellable=false;b.value.phase=GitUpdatePhase::written;form.tick(8);form.activate(9);TEST_ASSERT_EQUAL(1,b.installs);
  b.value.phase=GitUpdatePhase::error;b.value.reason=GitUpdateReason::activationFailed;form.render(view);TEST_ASSERT_NOT_NULL(strstr(view.hint,"Controls paused"));
  form.back(10);TEST_ASSERT_EQUAL(93,b.cancelled);TEST_ASSERT_EQUAL(0,pool.used());
}
static void changed_same_version_candidate_requires_a_new_review(){
  Backend b;b.available();JsonAllocator pool(4096,malloc,free);Updates::Form form(b,pool);
  form.open(0);form.turn(1,1);form.activate(2);form.turn(1,3);++b.value.candidate.generation;form.activate(4);
  TEST_ASSERT_EQUAL(0,b.installs);EntityAdmin::FormView view;form.render(view);TEST_ASSERT_EQUAL(EntityAdmin::Notice::changed,view.notice);
}
static void expiry_clock_lock_and_configuration_wipe_and_cancel_only_owned_operation(){
  for(unsigned reason=0;reason<4;++reason){
    Backend b;JsonAllocator pool(4096,malloc,free);Updates::Form form(b,pool);form.open(100);form.activate(101);
    uint64_t now=102;if(reason==0)b.auth.allowed=false;if(reason==1)++b.auth.configurationGeneration;
    if(reason==2)now=120101;if(reason==3)now=100;form.tick(now);
    TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_EQUAL(1,b.cancels);TEST_ASSERT_EQUAL(91,b.cancelled);TEST_ASSERT_EQUAL(0,pool.used());
  }
}
static void foreign_operation_is_not_cancelled_or_adopted(){
  Backend b;JsonAllocator pool(4096,malloc,free);Updates::Form form(b,pool);form.open(0);form.activate(1);
  b.value.operation=999;form.tick(2);form.back(3);TEST_ASSERT_EQUAL(0,b.cancels);TEST_ASSERT_EQUAL(0,b.installs);
}
static void unsupported_pending_boot_and_oom_never_start_work(){
  for(unsigned reason=0;reason<3;++reason){
    Backend b;if(reason==0)b.value.supported=false;if(reason==1)b.bootValue.pending=true;
    JsonAllocator pool(reason==2?1:4096,malloc,free);Updates::Form form(b,pool);
    const bool opened=form.open(0);if(reason==2)TEST_ASSERT_FALSE(opened);else{TEST_ASSERT_TRUE(opened);form.activate(1);}
    TEST_ASSERT_EQUAL(0,b.checks);TEST_ASSERT_EQUAL(0,b.installs);form.discard();TEST_ASSERT_EQUAL(0,pool.used());
  }
}
int main(){UNITY_BEGIN();RUN_TEST(check_is_explicit_and_progress_does_not_extend_authority);RUN_TEST(confirmation_defaults_cancel_and_installs_exact_candidate_once);RUN_TEST(changed_same_version_candidate_requires_a_new_review);RUN_TEST(expiry_clock_lock_and_configuration_wipe_and_cancel_only_owned_operation);RUN_TEST(foreign_operation_is_not_cancelled_or_adopted);RUN_TEST(unsupported_pending_boot_and_oom_never_start_work);return UNITY_END();}

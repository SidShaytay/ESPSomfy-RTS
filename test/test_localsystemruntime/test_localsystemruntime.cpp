#define ESPSOMFY_LOCAL_UI
#include "LocalSystem.h"
#include "SafeRestart.h"
#include <unity.h>
namespace LocalUi {
static SecuritySnapshot current{AuthMode::none,true,false,1,1};
static bool frozen=false;
SecuritySnapshot pollAuthorization(){return current;}
void authorizationActivity(){}
bool localControlsFrozen(){return frozen;}
}
static unsigned requests=0;
static RestartResult next{false,RestartReason::counterUncertain};
static RestartSnapshot status;
RestartResult requestRestart(uint32_t){++requests;return next;}
RestartSnapshot restartStatus(){return status;}
#include "../../src/LocalSystemRuntime.cpp"
void setUp(){LocalUi::current={LocalUi::AuthMode::none,true,false,1,1};LocalUi::frozen=false;requests=0;}
void tearDown(){}
static void final_authority_is_checked_before_shared_service() {
  auto &backend=LocalUi::System::runtimeBackend();
  TEST_ASSERT_FALSE(backend.restart(0,1).admitted);TEST_ASSERT_FALSE(backend.restart(1,0).admitted);
  LocalUi::current.generation=2;backend.restart(1,1);LocalUi::current.generation=1;
  LocalUi::current.configurationGeneration=2;backend.restart(1,1);LocalUi::current.configurationGeneration=1;
  LocalUi::current.allowed=false;backend.restart(1,1);LocalUi::current.allowed=true;
  LocalUi::frozen=true;backend.restart(1,1);LocalUi::frozen=false;
  TEST_ASSERT_EQUAL(0,requests);
  const auto result=backend.restart(1,1);TEST_ASSERT_EQUAL(1,requests);
  TEST_ASSERT_FALSE(result.admitted);TEST_ASSERT_EQUAL(RestartReason::counterUncertain,result.reason);
}
static void status_uses_authoritative_generation_and_phase() {
  status.phase=RestartPhase::cancelled;status.reason=RestartReason::modelUncertain;status.generation=17;
  const auto observed=LocalUi::System::runtimeBackend().status();
  TEST_ASSERT_EQUAL(17,observed.generation);TEST_ASSERT_EQUAL(RestartPhase::cancelled,observed.phase);
  TEST_ASSERT_EQUAL(RestartReason::modelUncertain,observed.reason);TEST_ASSERT_EQUAL(0,requests);
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(final_authority_is_checked_before_shared_service);RUN_TEST(status_uses_authoritative_generation_and_phase);return UNITY_END();}

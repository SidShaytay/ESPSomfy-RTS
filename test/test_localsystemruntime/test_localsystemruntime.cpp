#define ESPSOMFY_LOCAL_UI
#include "LocalSystem.h"
#include "SafeRestart.h"
#include <unity.h>
#include <string.h>
namespace LocalUi {
static SecuritySnapshot current{AuthMode::none,true,false,1,1};
static bool frozen=false;
SecuritySnapshot pollAuthorization(){return current;}
void authorizationActivity(){}
bool localControlsFrozen(){return frozen;}
bool readLocalNetworkRow(uint8_t,EntityAdmin::FormRow &out){out={};strcpy(out.label,"Connection");strcpy(out.value,"WiFi connected");return true;}
}
static unsigned requests=0;
static RestartResult next{false,RestartReason::counterUncertain};
static RestartSnapshot status;
RestartResult requestRestart(uint32_t){++requests;return next;}
RestartSnapshot restartStatus(){return status;}
#include "BatteryStatus.h"
static BatteryStatus::View power;static int8_t savedLimit=-1;
namespace BatteryStatus {
View view(){return power;}
bool setLimit(int8_t limit){if(!power.limitSupported)return false;savedLimit=limit;power.limit=limit;return true;}
}
#include "../../src/LocalSystemRuntime.cpp"
void setUp(){power={};savedLimit=-1;LocalUi::current={LocalUi::AuthMode::none,true,false,1,1};LocalUi::frozen=false;requests=0;}
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
static void battery_reads_and_writes_only_while_unlocked() {
  auto &backend=LocalUi::System::runtimeBackend();LocalUi::System::Backend::Battery b;
  power.reading={BatteryGauge::State::ready,64,3950,-12};power.limitSupported=true;power.limit=0;
  TEST_ASSERT_TRUE(backend.battery(b));TEST_ASSERT_TRUE(b.present);TEST_ASSERT_EQUAL(64,b.percent);TEST_ASSERT_EQUAL(3950,b.millivolts);
  TEST_ASSERT_TRUE(b.limitSupported);TEST_ASSERT_EQUAL(0,b.limit);
  TEST_ASSERT_TRUE(backend.setChargeLimit(4));TEST_ASSERT_EQUAL(4,savedLimit);
  TEST_ASSERT_FALSE(backend.setChargeLimit(5));
  LocalUi::current.allowed=false;savedLimit=-1;
  TEST_ASSERT_FALSE(backend.battery(b));TEST_ASSERT_FALSE(backend.setChargeLimit(3));TEST_ASSERT_EQUAL(-1,savedLimit);
  LocalUi::current.allowed=true;LocalUi::frozen=true;TEST_ASSERT_FALSE(backend.setChargeLimit(3));
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(battery_reads_and_writes_only_while_unlocked);RUN_TEST(final_authority_is_checked_before_shared_service);RUN_TEST(status_uses_authoritative_generation_and_phase);return UNITY_END();}

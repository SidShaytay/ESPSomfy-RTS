#include "LocalDiagnostics.h"
#include "LocalJsonAllocator.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>
using namespace LocalUi;
struct Backend : Diagnostics::Backend {
  ReceiveHub source;
  SecuritySnapshot auth{AuthMode::none,true,false,1,1};
  uint64_t time=0;
  unsigned activityCount=0;
  SecuritySnapshot authorization() override{return auth;}
  uint64_t now() override{return time;}
  void activity() override{++activityCount;}
  ReceiveHub &hub() override{return source;}
};
static void heard(Backend &b,unsigned at,unsigned code,ReceivePulses pulses={}) {
  b.time=at;ReceiveObservation frame;frame.observedAt=at;frame.address=0xabcdef;
  frame.rollingCode=code;frame.bits=56;frame.rssi=-65;frame.command=2;
  b.source.publish(frame,pulses); // Invalid frames remain useful diagnostics.
}
static void start(Diagnostics::Form &form){TEST_ASSERT_TRUE(form.open(0));form.turn(1,1);form.activate(2);}
void setUp(){}void tearDown(){}
static void default_cancel_and_bounded_copied_history_have_no_receiver_effect() {
  Backend b;JsonAllocator memory(4096,malloc,free);Diagnostics::Form form(b,memory);
  TEST_ASSERT_TRUE(form.open(0));TEST_ASSERT_FALSE(b.source.active());form.activate(1);
  TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_EQUAL(0,memory.used());
  start(form);TEST_ASSERT_TRUE(b.source.active());
  for(unsigned i=3;i<7;++i)heard(b,i,i);
  EntityAdmin::FormView view;form.render(view);
  TEST_ASSERT_EQUAL(5,view.total);TEST_ASSERT_EQUAL_STRING("#3",view.rows[0].label);
  TEST_ASSERT_EQUAL_STRING("#4",view.rows[1].label);TEST_ASSERT_TRUE(view.protectedFrame);
  form.discard();TEST_ASSERT_FALSE(b.source.active());TEST_ASSERT_EQUAL(0,memory.used());
}
static void selecting_freezes_all_32_bit_pulses_and_clear_only_wipes_view() {
  Backend b;JsonAllocator memory(4096,malloc,free);Diagnostics::Form form(b,memory);start(form);
  unsigned int pulses[300];for(unsigned i=0;i<300;++i)pulses[i]=i+10;pulses[0]=UINT32_MAX;
  heard(b,3,123,{pulses,300,false});pulses[0]=1;
  form.activate(4);TEST_ASSERT_FALSE(b.source.active());
  for(unsigned i=0;i<6;++i)form.turn(1,5);
  EntityAdmin::FormView metadata;form.render(metadata);
  TEST_ASSERT_EQUAL_STRING("Up (0x02)",metadata.rows[0].value);
  for(unsigned i=0;i<9;++i)form.turn(1,5);
  form.activate(6);EntityAdmin::FormView view;form.render(view);
  TEST_ASSERT_NOT_NULL(strstr(view.rows[0].value,"4294967295"));
  TEST_ASSERT_EQUAL(151,view.total);TEST_ASSERT_TRUE(view.protectedFrame);
  form.back(7);form.back(8); // Frozen frames, not automatic resubscription.
  TEST_ASSERT_FALSE(b.source.active());form.turn(1,9);form.turn(1,10); // Clear view.
  form.activate(11);form.render(view);TEST_ASSERT_EQUAL_STRING("Cancel",view.rows[0].label);
  form.activate(12);form.render(view);TEST_ASSERT_EQUAL(4,view.total); // Default Cancel kept frame.
  form.activate(13);form.turn(1,14);form.activate(15);form.render(view);
  TEST_ASSERT_EQUAL(3,view.total);TEST_ASSERT_FALSE(b.source.active());
  form.discard();TEST_ASSERT_EQUAL(0,memory.used());
}
static void expiry_auth_clock_and_capture_teardown_cancel_before_free() {
  for(unsigned reason=0;reason<4;++reason){
    Backend b;JsonAllocator memory(4096,malloc,free);Diagnostics::Form form(b,memory);start(form);
    if(reason==0)b.auth.allowed=false;
    if(reason==1)++b.auth.generation;
    if(reason==2){heard(b,120001,9);form.tick(120002);}
    else if(reason==3)form.tick(1);
    else heard(b,3,1); // Callback can revoke and destroy its own session safely.
    TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_FALSE(b.source.active());TEST_ASSERT_EQUAL(0,memory.used());
    heard(b,120003,2);TEST_ASSERT_EQUAL(0,memory.used());
  }
}
static void allocation_and_foreign_diagnostic_subscriptions_are_preserved() {
  Backend b;JsonAllocator none(1,malloc,free);Diagnostics::Form refused(b,none);
  TEST_ASSERT_FALSE(refused.open(0));TEST_ASSERT_FALSE(b.source.active());
  struct Sink:ReceiveSink{unsigned receivedCount=0;void received(const ReceiveObservation&,ReceivePulses)override{++receivedCount;}} foreign;
  ReceiveSubscription receipt;TEST_ASSERT_TRUE(b.source.subscribe(ReceiveChannel::diagnostics,foreign,receipt));
  JsonAllocator memory(4096,malloc,free);Diagnostics::Form form(b,memory);start(form);
  TEST_ASSERT_TRUE(b.source.observing(receipt,foreign));form.discard();heard(b,3,4);
  TEST_ASSERT_EQUAL(1,foreign.receivedCount);TEST_ASSERT_EQUAL(0,memory.used());
  TEST_ASSERT_TRUE(b.source.unsubscribe(receipt,foreign));
}
static void incomplete_timings_unknown_format_and_scan_flags_remain_honest() {
  Backend b;JsonAllocator memory(4096,malloc,free);Diagnostics::Form form(b,memory);start(form);
  b.time=3;ReceiveObservation frame;frame.observedAt=3;frame.protocol=255;frame.command=255;
  frame.flags=ReceiveObservation::validFlag|ReceiveObservation::scanningFlag;
  TEST_ASSERT_TRUE(b.source.publish(frame,{nullptr,99,false}));form.activate(4);
  for(unsigned i=0;i<4;++i)form.turn(1,5);EntityAdmin::FormView view;form.render(view);
  TEST_ASSERT_EQUAL_STRING("Unknown (255)",view.rows[1].value);
  form.turn(1,5);form.turn(1,5);form.render(view);TEST_ASSERT_EQUAL_STRING("Unknown (0xFF)",view.rows[0].value);
  for(unsigned i=0;i<4;++i)form.turn(1,5);form.render(view);
  TEST_ASSERT_EQUAL_STRING("Unavailable",view.rows[1].value);
  for(unsigned i=0;i<2;++i)form.turn(1,5);form.render(view);
  TEST_ASSERT_EQUAL_STRING("Yes",view.rows[0].value);TEST_ASSERT_EQUAL_STRING("Yes",view.rows[1].value);
  TEST_ASSERT_EQUAL_STRING("0 (clipped)",view.rows[2].value);
  for(unsigned i=0;i<3;++i)form.turn(1,5);form.activate(6);form.render(view);
  TEST_ASSERT_EQUAL(1,view.total);TEST_ASSERT_EQUAL_STRING("Back to frame",view.rows[0].label);
  form.discard();TEST_ASSERT_EQUAL(0,memory.used());
}
int main(){UNITY_BEGIN();RUN_TEST(incomplete_timings_unknown_format_and_scan_flags_remain_honest);RUN_TEST(default_cancel_and_bounded_copied_history_have_no_receiver_effect);RUN_TEST(selecting_freezes_all_32_bit_pulses_and_clear_only_wipes_view);RUN_TEST(expiry_auth_clock_and_capture_teardown_cancel_before_free);RUN_TEST(allocation_and_foreign_diagnostic_subscriptions_are_preserved);return UNITY_END();}

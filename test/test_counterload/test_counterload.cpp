#include "CounterLoad.h"
#include <nvs.h>
#include <unity.h>
#include <stdlib.h>
static unsigned frees=0;
static bool allocationFailed=false;
static void *allocate(size_t n){return allocationFailed?nullptr:malloc(n);}
static void release(void *p,size_t n){for(size_t i=0;i<n;++i)TEST_ASSERT_EQUAL(0,static_cast<uint8_t *>(p)[i]);++frees;free(p);}
static void stored(uint32_t address,uint16_t value){char key[32];snprintf(key,sizeof(key),"ShadeCodes/_%lu",static_cast<unsigned long>(address));FakeNvs::values[key]={FakeNvs::Type::u16,{uint8_t(value),uint8_t(value>>8)}};}
static uint16_t durable(uint32_t address){char key[32];snprintf(key,sizeof(key),"ShadeCodes/_%lu",static_cast<unsigned long>(address));auto &bytes=FakeNvs::values.at(key).bytes;return bytes[0]|uint16_t(bytes[1])<<8;}
void setUp(){FakeNvs::reset();frees=0;allocationFailed=false;}
void tearDown(){TEST_ASSERT_TRUE(FakeNvs::handles.empty());}
static void readonly_maxima_and_aliases_without_writes(){
 stored(100,500);stored(101,900);CounterLoad counters(allocate,release);
 TEST_ASSERT_TRUE(counters.include(100,20)&&counters.include(100,400)&&counters.include(101,0));
 TEST_ASSERT_TRUE(counters.resolve().ready());TEST_ASSERT_FALSE(counters.result().writeAttempted);uint16_t value=99;TEST_ASSERT_TRUE(counters.lookup(100,value));TEST_ASSERT_EQUAL(500,value);
 TEST_ASSERT_TRUE(counters.lookup(101,value));TEST_ASSERT_EQUAL(900,value);TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);TEST_ASSERT_EQUAL(0,FakeNvs::sets);TEST_ASSERT_EQUAL(1,FakeNvs::readOnlyOpens);
 counters.clear();TEST_ASSERT_EQUAL(1,frees);
}
static void promotions_reopen_verify_final_unique_maximum(){
 stored(100,10);CounterLoad counters(allocate,release);
 TEST_ASSERT_TRUE(counters.include(100,20)&&counters.include(100,300)&&counters.include(101,50));
 TEST_ASSERT_TRUE(counters.resolve().ready());TEST_ASSERT_TRUE(counters.result().writeAttempted);TEST_ASSERT_EQUAL(300,durable(100));TEST_ASSERT_EQUAL(50,durable(101));
 uint16_t value=0;TEST_ASSERT_TRUE(counters.lookup(100,value));TEST_ASSERT_EQUAL(300,value);
 TEST_ASSERT_EQUAL(2,FakeNvs::readOnlyOpens);TEST_ASSERT_EQUAL(1,FakeNvs::writeOpens);TEST_ASSERT_EQUAL(2,FakeNvs::sets);TEST_ASSERT_EQUAL(1,FakeNvs::commits);
 const auto reads=FakeNvs::reads;TEST_ASSERT_TRUE(counters.resolve().ready());TEST_ASSERT_EQUAL(reads,FakeNvs::reads);
}
static void every_read_fails_before_any_write_or_adoption(){
 for(int failure=1;failure<=3;++failure){FakeNvs::reset();CounterLoad counters(allocate,release);for(unsigned id=1;id<=3;++id)TEST_ASSERT_TRUE(counters.include(id,100));FakeNvs::failRead=failure;
  TEST_ASSERT_EQUAL(CounterLoadState::failed,counters.resolve().state);TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);TEST_ASSERT_EQUAL(0,FakeNvs::sets);uint16_t unchanged=777;TEST_ASSERT_FALSE(counters.lookup(1,unchanged));TEST_ASSERT_EQUAL(777,unchanged);}
}
static void exact_missing_namespace_and_keys_only(){
 CounterLoad missing(allocate,release);TEST_ASSERT_TRUE(missing.include(100,0));FakeNvs::failOpen=1;FakeNvs::openError=ESP_ERR_NVS_NOT_FOUND;
 TEST_ASSERT_TRUE(missing.resolve().ready());uint16_t value=8;TEST_ASSERT_TRUE(missing.lookup(100,value));TEST_ASSERT_EQUAL(0,value);TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);
 FakeNvs::reset();CounterLoad failed(allocate,release);TEST_ASSERT_TRUE(failed.include(100,3));FakeNvs::failOpen=1;
 TEST_ASSERT_EQUAL(CounterLoadState::failed,failed.resolve().state);TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);
 FakeNvs::reset();CounterLoad wrongType(allocate,release);FakeNvs::values["ShadeCodes/_100"]={FakeNvs::Type::u8,{9}};TEST_ASSERT_TRUE(wrongType.include(100,3));
 TEST_ASSERT_EQUAL(CounterLoadState::failed,wrongType.resolve().state);TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);
}
static void failures_after_write_attempt_are_uncertain_and_never_rollback(){
 for(unsigned phase=0;phase<5;++phase){FakeNvs::reset();stored(100,10);CounterLoad counters(allocate,release);TEST_ASSERT_TRUE(counters.include(100,300));
  if(phase==0)FakeNvs::failSet=1;if(phase==1)FakeNvs::failCommit=1;if(phase==2)FakeNvs::failOpen=3;if(phase==3)FakeNvs::failRead=2;if(phase==4)FakeNvs::corruptRead=2;
  TEST_ASSERT_EQUAL(CounterLoadState::uncertain,counters.resolve().state);TEST_ASSERT_TRUE(counters.result().writeAttempted);TEST_ASSERT_EQUAL(1,FakeNvs::sets);TEST_ASSERT_TRUE(durable(100)>=10);uint16_t unchanged=8;TEST_ASSERT_FALSE(counters.lookup(100,unchanged));TEST_ASSERT_EQUAL(8,unchanged);TEST_ASSERT_FALSE(counters.include(101,10));TEST_ASSERT_EQUAL(CounterLoadState::uncertain,counters.result().state);}
}
static void memory_capacity_address_and_empty_bounds(){
 CounterLoad empty(allocate,release);TEST_ASSERT_TRUE(empty.resolve().ready());TEST_ASSERT_EQUAL(0,FakeNvs::opens);
 allocationFailed=true;CounterLoad memory(allocate,release);TEST_ASSERT_FALSE(memory.include(1,1));TEST_ASSERT_EQUAL(CounterLoadStage::memory,memory.result().stage);TEST_ASSERT_EQUAL(0,FakeNvs::opens);allocationFailed=false;
 CounterLoad invalid(allocate,release);TEST_ASSERT_FALSE(invalid.include(0x1000000,1));TEST_ASSERT_EQUAL(0,FakeNvs::opens);
 CounterLoad maximum(allocate,release);for(unsigned id=1;id<=CounterLoad::maximumEntries;++id)TEST_ASSERT_TRUE(maximum.include(id,id));
 TEST_ASSERT_TRUE(maximum.resolve().ready());TEST_ASSERT_EQUAL(272,FakeNvs::sets);TEST_ASSERT_EQUAL(544,FakeNvs::reads);
 FakeNvs::reset();CounterLoad full(allocate,release);TEST_ASSERT_TRUE(full.include(0,0));for(unsigned id=1;id<=CounterLoad::maximumEntries;++id)TEST_ASSERT_TRUE(full.include(id,id));
 TEST_ASSERT_FALSE(full.include(CounterLoad::maximumEntries+1,1));TEST_ASSERT_EQUAL(0,FakeNvs::opens);
}
struct CancelContext {unsigned calls=0,stopAt=0,yields=0;int opens=0,reads=0,sets=0,commits=0;};
static void cooperate(const void *raw){auto &c=*const_cast<CancelContext *>(static_cast<const CancelContext *>(raw));++c.yields;}
static bool continuing(const void *raw){auto &c=*const_cast<CancelContext *>(static_cast<const CancelContext *>(raw));if(++c.calls!=c.stopAt)return true;c.opens=FakeNvs::opens;c.reads=FakeNvs::reads;c.sets=FakeNvs::sets;c.commits=FakeNvs::commits;return false;}
static void cancellation_bounds_every_io_and_never_replays_partial_promotion(){
 for(unsigned stop=1;stop<=8;++stop){FakeNvs::reset();stored(100,10);CounterLoad counters(allocate,release);TEST_ASSERT_TRUE(counters.include(100,300));CancelContext context;context.stopAt=stop;
  const auto result=counters.resolve({&context,continuing,cooperate});TEST_ASSERT_EQUAL(CounterLoadStage::interrupted,result.stage);
  TEST_ASSERT_EQUAL(stop>=5?CounterLoadState::uncertain:CounterLoadState::failed,result.state);TEST_ASSERT_EQUAL(stop>=5,result.writeAttempted);
  TEST_ASSERT_EQUAL(context.calls,context.yields);TEST_ASSERT_EQUAL(context.opens,FakeNvs::opens);TEST_ASSERT_EQUAL(context.reads,FakeNvs::reads);TEST_ASSERT_EQUAL(context.sets,FakeNvs::sets);TEST_ASSERT_EQUAL(context.commits,FakeNvs::commits);
  TEST_ASSERT_TRUE(FakeNvs::handles.empty());uint16_t value=77;TEST_ASSERT_FALSE(counters.lookup(100,value));TEST_ASSERT_EQUAL(77,value);
  TEST_ASSERT_EQUAL(result.stage,counters.resolve().stage);TEST_ASSERT_EQUAL(context.sets,FakeNvs::sets);
 }
}
int main(){UNITY_BEGIN();RUN_TEST(cancellation_bounds_every_io_and_never_replays_partial_promotion);RUN_TEST(readonly_maxima_and_aliases_without_writes);RUN_TEST(promotions_reopen_verify_final_unique_maximum);RUN_TEST(every_read_fails_before_any_write_or_adoption);RUN_TEST(exact_missing_namespace_and_keys_only);RUN_TEST(failures_after_write_attempt_are_uncertain_and_never_rollback);RUN_TEST(memory_capacity_address_and_empty_bounds);return UNITY_END();}

#include <unity.h>
#include "FrequencyScanSession.h"
using namespace FrequencyScan;
struct Fake:Backend {
 bool trusted=true,held=false,running=false,canBegin=true,canRelease=true;unsigned stops=0,releases=0;uint64_t generation=0;
 bool current(const Source&)override{return trusted;}
 bool acquire(GitUpdateOwnerToken &out)override{if(held)return false;held=true;out={++generation,GitUpdateExternalOwner::configuration};return true;}
 bool owns(const GitUpdateOwnerToken &owner)override{return held&&owner.generation==generation;}
 bool begin()override{if(!canBegin)return false;running=true;return true;}
 void stop()override{++stops;}
 bool active()override{return running;}
 bool sample(Sample &out)override{out={433.42f,433.43f,-62,-55};return true;}
 bool release(const GitUpdateOwnerToken&)override{if(!canRelease)return false;++releases;held=false;return true;}
};
void setUp(){}void tearDown(){}
static Source web(uint32_t client=7){return {SourceKind::web,client,0,0,1,2};}
static void exact_source_and_token_stop_wait_for_real_restoration(){
 Fake backend;Session session(backend,42);FrequencyScanToken token;
 TEST_ASSERT_TRUE(session.start(web(),100,token));TEST_ASSERT_TRUE(backend.held);
 auto foreign=token;++foreign.generation;
 TEST_ASSERT_FALSE(session.stop(foreign,web(),101));TEST_ASSERT_FALSE(session.stop(token,web(8),102));
 TEST_ASSERT_EQUAL(0,backend.stops);TEST_ASSERT_TRUE(session.stop(token,web(),103));
 TEST_ASSERT_EQUAL(Phase::stopping,session.snapshot().phase);session.poll(104);TEST_ASSERT_TRUE(backend.held);
 backend.running=false;session.poll(105);TEST_ASSERT_FALSE(backend.held);TEST_ASSERT_EQUAL(Phase::review,session.snapshot().phase);
 FrequencyScanToken replacement;TEST_ASSERT_TRUE(session.start(web(),106,replacement));TEST_ASSERT_FALSE(token.same(replacement));
 TEST_ASSERT_FALSE(session.stop(token,web(),107));TEST_ASSERT_EQUAL(1,backend.stops);
}
static void expiry_clock_and_auth_stop_without_releasing_active_radio(){
 for(unsigned failure=0;failure<3;++failure){Fake backend;Session session(backend,42);FrequencyScanToken token;
 TEST_ASSERT_TRUE(session.start(web(),100,token));if(failure==0)backend.trusted=false;
 session.poll(failure==1?99:failure==2?300100:101);TEST_ASSERT_EQUAL(Phase::stopping,session.snapshot().phase);
 TEST_ASSERT_EQUAL(1,backend.stops);TEST_ASSERT_TRUE(backend.held);
 backend.running=false;session.poll(300101);TEST_ASSERT_FALSE(backend.held);}
}
static void refused_start_and_lost_owner_never_stop_foreign_scan(){
 Fake backend;Session session(backend,42);FrequencyScanToken token{8,9};backend.canBegin=false;
 TEST_ASSERT_FALSE(session.start(web(),100,token));TEST_ASSERT_EQUAL(8,token.bootSession);TEST_ASSERT_FALSE(backend.held);TEST_ASSERT_EQUAL(0,backend.stops);
 backend.canBegin=true;TEST_ASSERT_TRUE(session.start(web(),101,token));++backend.generation;session.poll(102);
 TEST_ASSERT_EQUAL(Phase::attention,session.snapshot().phase);TEST_ASSERT_EQUAL(0,backend.stops);TEST_ASSERT_TRUE(backend.running);
}
static void reviewed_peak_is_context_bound_and_never_a_configuration_write(){
 Fake backend;Session session(backend,42);FrequencyScanToken token;float frequency=99;
 TEST_ASSERT_TRUE(session.start(web(),100,token));TEST_ASSERT_FALSE(session.peak(token,web(),frequency));
 session.poll(101);TEST_ASSERT_TRUE(session.snapshot().sampleValid);
 TEST_ASSERT_TRUE(session.stop(token,web(),102));backend.running=false;session.poll(103);
 TEST_ASSERT_TRUE(session.peak(token,web(),frequency));TEST_ASSERT_FLOAT_WITHIN(.0001f,433.43f,frequency);
 backend.trusted=false;frequency=99;TEST_ASSERT_FALSE(session.peak(token,web(),frequency));TEST_ASSERT_EQUAL_FLOAT(99,frequency);
}
static void restored_authority_cannot_revive_clock_or_stale_review(){
 for(unsigned failure=0;failure<2;++failure){Fake backend;Session session(backend,42);FrequencyScanToken token;
 TEST_ASSERT_TRUE(session.start(web(),100,token));session.poll(101);TEST_ASSERT_TRUE(session.snapshot().sampleValid);
 if(!failure)backend.trusted=false;session.poll(failure?99:102);
 backend.running=false;backend.trusted=true;session.poll(103);
 float frequency=99;TEST_ASSERT_FALSE(session.peak(token,web(),frequency));TEST_ASSERT_EQUAL_FLOAT(99,frequency);
 }
}
int main(){UNITY_BEGIN();RUN_TEST(restored_authority_cannot_revive_clock_or_stale_review);RUN_TEST(reviewed_peak_is_context_bound_and_never_a_configuration_write);RUN_TEST(exact_source_and_token_stop_wait_for_real_restoration);RUN_TEST(expiry_clock_and_auth_stop_without_releasing_active_radio);RUN_TEST(refused_start_and_lost_owner_never_stop_foreign_scan);return UNITY_END();}

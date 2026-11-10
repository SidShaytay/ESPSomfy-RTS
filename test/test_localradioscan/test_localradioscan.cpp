#include "LocalFrequencyScan.h"
#include "LocalJsonAllocator.h"
#include <unity.h>
#include <cstdlib>
#include <cstring>
using namespace LocalUi;
struct Fake:RadioScan::Backend {
 SecuritySnapshot auth{AuthMode::none,true,false,1,1};uint64_t radioHistory=1;unsigned starts=0,stops=0,activityCount=0;
 ::FrequencyScan::Snapshot view;
 SecuritySnapshot authorization()override{return auth;}
 void activity()override{++activityCount;}
 ::FrequencyScan::Source capture()override{return {::FrequencyScan::SourceKind::lcd,0,auth.generation,auth.configurationGeneration,1,radioHistory};}
 bool begin(const ::FrequencyScan::Source &,FrequencyScanToken &out)override{++starts;view.token={42,starts};view.phase=::FrequencyScan::Phase::scanning;out=view.token;return true;}
 bool stop(const ::FrequencyScan::Source &,const FrequencyScanToken &token)override{if(!token.same(view.token))return false;++stops;view.phase=::FrequencyScan::Phase::stopping;return true;}
 ::FrequencyScan::Snapshot status()override{return view;}
 bool peak(const ::FrequencyScan::Source &,const FrequencyScanToken &token,float &out)override{if(view.phase!=::FrequencyScan::Phase::review||!token.same(view.token))return false;out=433.42f;return true;}
};
void setUp(){}void tearDown(){}
static void *allocate(size_t size){return malloc(size);}static void release(void *memory){free(memory);}
static void entry_cancel_and_timeout_never_start_or_copy(){
 Fake backend;JsonAllocator memory(4096,allocate,release);RadioScan::Form form(backend,memory);EntityAdmin::FormView view;
 TEST_ASSERT_TRUE(form.open(0));form.render(view);TEST_ASSERT_EQUAL_STRING("Cancel",view.rows[0].label);TEST_ASSERT_EQUAL(0,view.selected);
 form.activate(1);TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_EQUAL(0,backend.starts);TEST_ASSERT_EQUAL(0,memory.used());
 TEST_ASSERT_TRUE(form.open(2));form.tick(120002);TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_EQUAL(0,backend.starts);
}
static void explicit_start_stop_review_and_copy_is_separate_from_save(){
 Fake backend;JsonAllocator memory(4096,allocate,release);RadioScan::Form form(backend,memory);EntityAdmin::FormView view;
 TEST_ASSERT_TRUE(form.open(0));form.turn(1,1);form.activate(2);TEST_ASSERT_EQUAL(1,backend.starts);
 form.tick(3);TEST_ASSERT_EQUAL(2,backend.activityCount);form.activate(4);TEST_ASSERT_EQUAL(1,backend.stops);
 backend.view.phase=::FrequencyScan::Phase::review;backend.view.sampleValid=true;backend.view.sample.peakRssi=-55;backend.view.sample.peakFrequency=433.42f;
 form.tick(5);form.render(view);TEST_ASSERT_EQUAL_STRING("Close",view.rows[0].label);
 // Three sample/action rows require the renderer layout with reserved help space.
 TEST_ASSERT_EQUAL(EntityAdmin::Mode::stackedRows,view.mode);TEST_ASSERT_EQUAL(3,view.rowCount);
 form.turn(1,6);form.activate(7);float value=0;SecuritySnapshot expected;uint64_t radio=0;
 TEST_ASSERT_TRUE(form.takeFrequency(value,expected,radio,8));TEST_ASSERT_FLOAT_WITHIN(.0001f,433.42f,value);TEST_ASSERT_EQUAL(1,radio);TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_EQUAL(0,memory.used());
 TEST_ASSERT_FALSE(form.takeFrequency(value,expected,radio,9));
}
static void leaving_active_or_losing_context_stops_only_owned_scan(){
 for(unsigned i=0;i<3;++i){Fake backend;JsonAllocator memory(4096,allocate,release);RadioScan::Form form(backend,memory);
 TEST_ASSERT_TRUE(form.open(0));form.turn(1,1);form.activate(2);
 if(i==0)form.back(3);if(i==1){backend.auth.allowed=false;form.tick(3);}if(i==2){++backend.radioHistory;form.tick(3);}
 TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_EQUAL(1,backend.stops);TEST_ASSERT_EQUAL(0,memory.used());}
}
int main(){UNITY_BEGIN();RUN_TEST(entry_cancel_and_timeout_never_start_or_copy);RUN_TEST(explicit_start_stop_review_and_copy_is_separate_from_save);RUN_TEST(leaving_active_or_losing_context_stops_only_owned_scan);return UNITY_END();}

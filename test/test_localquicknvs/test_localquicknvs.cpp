#include <unity.h>
#include "LocalQuickAccessNvs.h"
#include <nvs.h>
#include "../../src/LocalQuickAccessNvs.cpp"
using namespace LocalUi;
void setUp(){FakeNvs::reset();}void tearDown(){}
struct Witness:QuickPreferenceWitness {bool allowed=true;unsigned calls=0;bool current()override{++calls;return allowed;}};
static QuickAccess draft(){QuickAccess q;q.assign(0,{EntityKind::group,1},0x010203);return q;}
static void read_never_creates_and_distinguishes_missing_corrupt_unreadable() {
 QuickAccess q;TEST_ASSERT_EQUAL(QuickAccessLoad::missing,readQuickAccessPreferences(q));
 TEST_ASSERT_EQUAL(0,FakeNvs::sets);TEST_ASSERT_EQUAL(0,FakeNvs::commits);TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);
 FakeNvs::reset();FakeNvs::failOpen=1;TEST_ASSERT_EQUAL(QuickAccessLoad::unreadable,readQuickAccessPreferences(q));
 FakeNvs::reset();nvs_handle_t h;nvs_open("LCDUI",NVS_READWRITE,&h);nvs_set_str(h,"quickV1","bad");nvs_close(h);
 TEST_ASSERT_EQUAL(QuickAccessLoad::invalid,readQuickAccessPreferences(q));
}
static void explicit_save_commits_and_freshly_verifies_only_its_key() {
 FakeNvs::values["LCDUI/brightness"]={FakeNvs::Type::u8,{32}};
 Witness w;auto q=draft();TEST_ASSERT_EQUAL(QuickPreferenceSave::saved,writeQuickAccessPreferences(q,w));
 TEST_ASSERT_TRUE(w.calls>=2);TEST_ASSERT_EQUAL(1,FakeNvs::commits);TEST_ASSERT_EQUAL(1,FakeNvs::sets);
 TEST_ASSERT_EQUAL(32,FakeNvs::values["LCDUI/brightness"].bytes[0]);TEST_ASSERT_EQUAL(0,FakeNvs::erases);
 QuickAccess read;TEST_ASSERT_EQUAL(QuickAccessLoad::valid,readQuickAccessPreferences(read));
 TEST_ASSERT_TRUE(read.at(0)->ref==q.at(0)->ref);TEST_ASSERT_EQUAL_UINT32(0x010203,read.at(0)->identity);
}
static void save_errors_never_claim_verified_or_write_without_witness() {
 for(unsigned fault=0;fault<5;++fault) {
  FakeNvs::reset();Witness w;
  if(fault==0)w.allowed=false;
  if(fault==1)FakeNvs::failOpen=1;
  if(fault==2)FakeNvs::failSet=1;
  if(fault==3)FakeNvs::failCommit=1;
  if(fault==4)FakeNvs::failRead=1;
  const auto r=writeQuickAccessPreferences(draft(),w);TEST_ASSERT_NOT_EQUAL(QuickPreferenceSave::saved,r);
  if(fault<2)TEST_ASSERT_EQUAL(0,FakeNvs::sets);
  if(fault>=2)TEST_ASSERT_EQUAL(QuickPreferenceSave::unknown,r);
 }
}
static void every_storage_boundary_rechecks_fresh_authority() {
 struct Revoking:QuickPreferenceWitness {unsigned call=0, revoke;explicit Revoking(unsigned n):revoke(n){}bool current()override{return ++call<revoke;}};
 for(unsigned at=2;at<=5;++at) {
  FakeNvs::reset();FakeNvs::values["LCDUI/brightness"]={FakeNvs::Type::u8,{32}};
  Revoking witness(at);const auto result=writeQuickAccessPreferences(draft(),witness);
  TEST_ASSERT_EQUAL(at==2?QuickPreferenceSave::failed:QuickPreferenceSave::unknown,result);
  TEST_ASSERT_EQUAL(at==2?0:1,FakeNvs::sets);
  TEST_ASSERT_EQUAL(at<=3?0:1,FakeNvs::commits);
  TEST_ASSERT_EQUAL(at==5?2:0,FakeNvs::reads);
  TEST_ASSERT_EQUAL(FakeNvs::opens,FakeNvs::closes);TEST_ASSERT_TRUE(FakeNvs::handles.empty());
  TEST_ASSERT_EQUAL(32,FakeNvs::values["LCDUI/brightness"].bytes[0]);TEST_ASSERT_EQUAL(0,FakeNvs::erases);
 }
}
int main(){UNITY_BEGIN();RUN_TEST(every_storage_boundary_rechecks_fresh_authority);RUN_TEST(read_never_creates_and_distinguishes_missing_corrupt_unreadable);RUN_TEST(explicit_save_commits_and_freshly_verifies_only_its_key);RUN_TEST(save_errors_never_claim_verified_or_write_without_witness);return UNITY_END();}

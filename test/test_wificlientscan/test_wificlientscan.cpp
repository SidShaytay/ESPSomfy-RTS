#include <unity.h>
#include "WifiClientScan.h"
#include "GitUpdate.h"
#include <string.h>
#include "../../src/GitUpdate.cpp"
// Compile this owner-using runtime only in its integration test; other native
// executables do not link the device updater singleton.
#include "../../src/WifiClientScan.cpp"
struct Driver : WifiScanDriver {
  bool allowed=true,started=true;int completion=-1;unsigned starts=0,clears=0,broadcasts=0;int sent=99;
  bool mayStart() const override{return allowed;}
  bool start() override{++starts;return started;}
  int result() override{return completion;}
  void broadcast(int n) override{++broadcasts;sent=n;}
  void clearResults() override{++clears;}
  uint16_t count() const override{return completion>0?completion:0;}
  bool read(uint16_t,WifiScanNetwork &) const override{return false;}
};
struct Sink {unsigned calls=0;uint64_t observed=0;bool ready=false;static void done(void *context,uint64_t token,bool success,const WifiScanReader &){auto &s=*static_cast<Sink*>(context);++s.calls;s.observed=token;s.ready=success;}};
void setUp(){TEST_ASSERT_FALSE(gitUpdate.busy());}void tearDown(){TEST_ASSERT_FALSE(gitUpdate.busy());}
static void busy_or_connecting_never_start_and_accepted_scan_holds_owner() {
  WifiScanFeed feed;WifiClientScan scan(feed);Driver driver;Sink sink;
  auto token=feed.request(Sink::done,&sink);driver.allowed=false;scan.loop(driver,100);TEST_ASSERT_EQUAL(0,driver.starts);
  driver.allowed=true;GitUpdateOwnerToken other;TEST_ASSERT_TRUE(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,other));
  scan.loop(driver,100);TEST_ASSERT_EQUAL(0,driver.starts);TEST_ASSERT_TRUE(gitUpdate.releaseExternal(other));
  scan.loop(driver,100);TEST_ASSERT_EQUAL(1,driver.starts);TEST_ASSERT_TRUE(scan.active());TEST_ASSERT_TRUE(gitUpdate.busy());
  TEST_ASSERT_FALSE(gitUpdate.acquireExternal(GitUpdateExternalOwner::counter,other));scan.loop(driver,100);TEST_ASSERT_EQUAL(0,driver.broadcasts);
  driver.completion=2;scan.loop(driver,100);TEST_ASSERT_EQUAL(1,sink.calls);TEST_ASSERT_EQUAL_UINT64(token,sink.observed);TEST_ASSERT_TRUE(sink.ready);
  TEST_ASSERT_EQUAL(1,driver.clears);TEST_ASSERT_FALSE(scan.active());
}
static void cancellation_does_not_let_a_late_scan_inherit_new_source() {
  WifiScanFeed feed;WifiClientScan scan(feed);Driver driver;Sink sink;
  auto first=feed.request(Sink::done,&sink);scan.loop(driver,100);TEST_ASSERT_TRUE(feed.cancel(first));
  auto second=feed.request(Sink::done,&sink);driver.completion=1;scan.loop(driver,100);TEST_ASSERT_EQUAL(0,sink.calls);TEST_ASSERT_TRUE(feed.pending());
  driver.completion=-1;scan.loop(driver,100);TEST_ASSERT_EQUAL(2,driver.starts);driver.completion=0;scan.loop(driver,100);
  TEST_ASSERT_EQUAL(1,sink.calls);TEST_ASSERT_EQUAL_UINT64(second,sink.observed);
}
static void failed_start_never_clears_foreign_sdk_storage() {
  WifiScanFeed feed;WifiClientScan scan(feed);Driver driver;Sink sink;driver.started=false;
  feed.request(Sink::done,&sink);scan.loop(driver,100);TEST_ASSERT_EQUAL(1,driver.starts);TEST_ASSERT_EQUAL(0,driver.clears);
  TEST_ASSERT_EQUAL(1,sink.calls);TEST_ASSERT_FALSE(sink.ready);TEST_ASSERT_FALSE(feed.pending());scan.loop(driver,100);TEST_ASSERT_EQUAL(1,driver.starts);
}
static void web_request_during_active_scan_does_not_start_a_second_scan() {
  WifiScanFeed feed;WifiClientScan scan(feed);Driver driver;scan.requestWeb(100);scan.loop(driver,100);
  scan.requestWeb(100);driver.completion=0;scan.loop(driver,100);scan.loop(driver,100);
  TEST_ASSERT_EQUAL(1,driver.starts);TEST_ASSERT_FALSE(scan.pending());
}
static void expired_or_backward_web_request_never_starts_later() {
  for(unsigned backwards=0;backwards<2;++backwards) {
    WifiScanFeed feed;WifiClientScan scan(feed);Driver driver;
    scan.requestWeb(100);driver.allowed=false;scan.loop(driver,100);
    driver.allowed=true;scan.loop(driver,backwards?99:30100);
    TEST_ASSERT_EQUAL(0,driver.starts);TEST_ASSERT_EQUAL(1,driver.broadcasts);TEST_ASSERT_EQUAL(-2,driver.sent);
    TEST_ASSERT_FALSE(scan.pending());TEST_ASSERT_FALSE(gitUpdate.busy());
  }
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(busy_or_connecting_never_start_and_accepted_scan_holds_owner);RUN_TEST(cancellation_does_not_let_a_late_scan_inherit_new_source);RUN_TEST(failed_start_never_clears_foreign_sdk_storage);RUN_TEST(web_request_during_active_scan_does_not_start_a_second_scan);RUN_TEST(expired_or_backward_web_request_never_starts_later);return UNITY_END();}

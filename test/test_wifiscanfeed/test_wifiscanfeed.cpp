#include <unity.h>
#include "WifiScanFeed.h"
#include <string.h>
struct Reader : WifiScanReader {
  uint16_t count() const override{return 1;}
  bool read(uint16_t index,WifiScanNetwork &out) const override {if(index)return false;out={};strcpy(out.ssid,"Office");return true;}
};
struct Sink {
  WifiScanFeed &feed;unsigned calls=0;uint64_t observed=0,next=0;bool success=false,reregister=false;
  static void completed(void *context,uint64_t token,bool ready,const WifiScanReader &reader) {
    auto &sink=*static_cast<Sink*>(context);++sink.calls;sink.observed=token;sink.success=ready;
    WifiScanNetwork row;TEST_ASSERT_EQUAL(1,reader.count());TEST_ASSERT_TRUE(reader.read(0,row));TEST_ASSERT_EQUAL_STRING("Office",row.ssid);
    if(sink.reregister)sink.next=sink.feed.request(completed,&sink);
  }
};
void setUp(){}void tearDown(){}
static void generation_exhaustion_and_invalid_subscription_fail_closed() {
  TEST_ASSERT_EQUAL_UINT64(UINT64_MAX,WifiScanFeed::nextGeneration(UINT64_MAX-1));
  TEST_ASSERT_EQUAL_UINT64(0,WifiScanFeed::nextGeneration(UINT64_MAX));
  WifiScanFeed feed;Sink sink{feed};TEST_ASSERT_EQUAL(0,feed.request(nullptr,&sink));
  TEST_ASSERT_EQUAL(0,feed.request(Sink::completed,nullptr));TEST_ASSERT_FALSE(feed.pending());
}
static void exact_cancel_and_completion_never_reuse_a_listener() {
  WifiScanFeed feed;Sink sink{feed};Reader reader;
  auto first=feed.request(Sink::completed,&sink);TEST_ASSERT_NOT_EQUAL(0,first);TEST_ASSERT_TRUE(feed.pending());
  TEST_ASSERT_EQUAL(0,feed.request(Sink::completed,&sink));TEST_ASSERT_FALSE(feed.cancel(first+1));
  TEST_ASSERT_TRUE(feed.cancel(first));feed.finish(first,true,reader);TEST_ASSERT_EQUAL(0,sink.calls);
  auto second=feed.request(Sink::completed,&sink);feed.finish(first,true,reader);TEST_ASSERT_EQUAL(0,sink.calls);TEST_ASSERT_TRUE(second>first);TEST_ASSERT_FALSE(feed.cancel(first));
  feed.finish(second,false,reader);TEST_ASSERT_EQUAL(1,sink.calls);TEST_ASSERT_EQUAL(second,sink.observed);TEST_ASSERT_FALSE(sink.success);TEST_ASSERT_FALSE(feed.pending());
  feed.finish(second,true,reader);TEST_ASSERT_EQUAL(1,sink.calls);
}
static void completion_can_install_a_new_listener_without_old_cleanup() {
  WifiScanFeed feed;Sink sink{feed};Reader reader;sink.reregister=true;
  auto token=feed.request(Sink::completed,&sink);feed.finish(token,true,reader);
  TEST_ASSERT_EQUAL(1,sink.calls);TEST_ASSERT_TRUE(sink.next>token);TEST_ASSERT_TRUE(feed.pending());
  TEST_ASSERT_FALSE(feed.cancel(token));TEST_ASSERT_TRUE(feed.cancel(sink.next));
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(generation_exhaustion_and_invalid_subscription_fail_closed);RUN_TEST(exact_cancel_and_completion_never_reuse_a_listener);RUN_TEST(completion_can_install_a_new_listener_without_old_cleanup);return UNITY_END();}

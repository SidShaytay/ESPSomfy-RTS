#include "SettingsRevisions.h"
#include "ProtoSettings.h"
#include <mutex>
#include <future>
#include <atomic>
#include <cassert>
#include "history.inc"
int main(){
 const auto section=settings_section_t::security;
 assert(securityConfigurationRevision()==1);noteDeviceSettingsCompleted(section);
 assert(securityConfigurationRevision()==2);invalidateDeviceSettingsRevisions();assert(securityConfigurationRevision()==3);
 std::atomic<bool> started{false};std::future<uint64_t> read;
 {
  SettingsHistoryDetail::Guard held;
  read=std::async(std::launch::async,[&]{started=true;return securityConfigurationRevision();});
  while(!started.load())std::this_thread::yield();
  assert(read.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout);
 }
 assert(read.get()==3);
 std::atomic<bool> done{false};
 auto observer=std::async(std::launch::async,[&]{uint64_t previous=3;while(!done.load()){const auto current=securityConfigurationRevision();assert(current>=previous);previous=current;}});
 for(unsigned i=0;i<200000;++i)noteDeviceSettingsCompleted(section);
 done=true;observer.get();assert(securityConfigurationRevision()==200003);
 assert(deviceSettingsRevision(settings_section_t::general)==2);
}

#include <unity.h>
#include <vector>
#include <string>
#include "SettingsCompletion.h"
#include "SettingsRevisions.h"
struct ClientNotifications {
  const std::vector<uint32_t> clients{11,22,33};
  std::vector<uint32_t> closed,notified;
  uint32_t deferredOrigin=0;
  std::string section;
  int closeCalls=0,changeCalls=0;
  const rebootDelay_t *observedRestart=nullptr;
  bool rebootDuringNotification=false;
  void closeAll(uint32_t origin) {
    ++closeCalls;
    for(auto id:clients) if(id!=origin) closed.push_back(id);
    deferredOrigin=origin;
  }
  void settingsChanged(uint32_t origin,const char *name) {
    ++changeCalls;
    if(observedRestart) rebootDuringNotification=observedRestart->reboot;
    for(auto id:clients) if(id!=origin) notified.push_back(id);
    section=name;
  }
};
void setUp() {}
void tearDown() {}
static void test_local_security_save_revokes_all_clients() {
  ClientNotifications clients;
  rebootDelay_t restart;
  completeSettingsSave({settings_section_t::security,security_types::None,security_types::Password},clients,restart,100);
  TEST_ASSERT_EQUAL(1,clients.closeCalls);
  TEST_ASSERT_EQUAL(3,clients.closed.size());
  TEST_ASSERT_EQUAL(0,clients.deferredOrigin);
  TEST_ASSERT_EQUAL(0,clients.changeCalls);
  TEST_ASSERT_FALSE(restart.reboot);
}
static void test_socket_security_save_preserves_origin_until_reply_drains() {
  ClientNotifications clients;
  rebootDelay_t restart;
  completeSettingsSave({settings_section_t::security,security_types::Password,security_types::Password,22},clients,restart,100);
  TEST_ASSERT_EQUAL(1,clients.closeCalls);
  TEST_ASSERT_EQUAL(2,clients.closed.size());
  TEST_ASSERT_EQUAL(11,clients.closed[0]);
  TEST_ASSERT_EQUAL(33,clients.closed[1]);
  TEST_ASSERT_EQUAL(22,clients.deferredOrigin);
  TEST_ASSERT_EQUAL(0,clients.changeCalls);
}
static void test_every_security_transition_reauthenticates_except_none_to_none() {
  for(uint8_t before=0;before<3;before++) for(uint8_t after=0;after<3;after++) {
    ClientNotifications clients;
    rebootDelay_t restart;
    completeSettingsSave({settings_section_t::security,security_types(before),security_types(after)},clients,restart,1);
    const bool reauth=before!=0 || after!=0;
    TEST_ASSERT_EQUAL(reauth ? 1 : 0,clients.closeCalls);
    TEST_ASSERT_EQUAL(reauth ? 0 : 1,clients.changeCalls);
    if(!reauth) TEST_ASSERT_EQUAL_STRING("security",clients.section.c_str());
  }
}
static void test_changed_notification_excludes_socket_origin_but_local_notifies_everyone() {
  for(uint32_t origin:{0u,22u}) {
    ClientNotifications clients;
    rebootDelay_t restart;
    completeSettingsSave({settings_section_t::general,security_types::Password,security_types::Password,origin},clients,restart,1);
    TEST_ASSERT_EQUAL(0,clients.closeCalls);
    TEST_ASSERT_EQUAL(1,clients.changeCalls);
    TEST_ASSERT_EQUAL(origin ? 2 : 3,clients.notified.size());
    TEST_ASSERT_EQUAL_STRING("general",clients.section.c_str());
    for(auto id:clients.notified) TEST_ASSERT_NOT_EQUAL(origin,id);
    TEST_ASSERT_FALSE(restart.reboot);
  }
}
static void test_reboot_is_scheduled_after_notifications_only_when_requested() {
  ClientNotifications clients;
  rebootDelay_t restart;
  clients.observedRestart=&restart;
  completeSettingsSave({settings_section_t::network,security_types::None,security_types::None,0,true},clients,restart,1234);
  TEST_ASSERT_EQUAL(1,clients.changeCalls);
  TEST_ASSERT_TRUE(restart.reboot);
  TEST_ASSERT_FALSE(clients.rebootDuringNotification);
  TEST_ASSERT_EQUAL(2234,restart.rebootTime);
  TEST_ASSERT_FALSE(restart.closed);
}
static void test_section_history_is_bounded_and_invalid_sections_are_safe() {
  SettingsRevisions revisions;
  TEST_ASSERT_EQUAL_UINT64(0,revisions.get(settings_section_t(255)));
  revisions.completed(settings_section_t(255));
  revisions.completed(settings_section_t::mqtt);
  for(uint8_t section=0;section<5;section++)
    TEST_ASSERT_EQUAL_UINT64(section==static_cast<uint8_t>(settings_section_t::mqtt) ? 2 : 1,revisions.get(settings_section_t(section)));
}
static void test_restore_style_invalidation_advances_every_section() {
  SettingsRevisions revisions;
  revisions.completed(settings_section_t::radio);
  revisions.invalidateAll();
  for(uint8_t section=0;section<5;section++)
    TEST_ASSERT_EQUAL_UINT64(section==static_cast<uint8_t>(settings_section_t::radio) ? 3 : 2,revisions.get(settings_section_t(section)));
  TEST_ASSERT_EQUAL_UINT64(0,revisions.get(settings_section_t(5)));
}
static void generic_completion_never_overwrites_an_armed_manual_source() {
  ClientNotifications clients;rebootDelay_t restart;
  restart.reboot=true;restart.rebootTime=800;
  restart.externalOwnerGeneration=77;restart.externalOwnerKind=0;
  completeSettingsSave({settings_section_t::network,security_types::None,security_types::None,0,true},clients,restart,1);
  TEST_ASSERT_EQUAL(800,restart.rebootTime);
  TEST_ASSERT_EQUAL_UINT64(77,restart.externalOwnerGeneration);
  TEST_ASSERT_EQUAL(1,clients.changeCalls);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(generic_completion_never_overwrites_an_armed_manual_source);
  RUN_TEST(test_local_security_save_revokes_all_clients);
  RUN_TEST(test_socket_security_save_preserves_origin_until_reply_drains);
  RUN_TEST(test_every_security_transition_reauthenticates_except_none_to_none);
  RUN_TEST(test_changed_notification_excludes_socket_origin_but_local_notifies_everyone);
  RUN_TEST(test_reboot_is_scheduled_after_notifications_only_when_requested);
  RUN_TEST(test_section_history_is_bounded_and_invalid_sections_are_safe);
  RUN_TEST(test_restore_style_invalidation_advances_every_section);
  return UNITY_END();
}

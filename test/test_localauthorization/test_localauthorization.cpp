#include <unity.h>
#include "LocalAuthorization.h"
#include "LocalCredentialEditor.h"
#include "LocalStopLease.h"
#include <string.h>
using namespace LocalUi;
void setUp() {}
void tearDown() {}
static Credentials input(const char *user, const char *secret) {
  Credentials result;
  strcpy(result.username, user);
  strcpy(result.secret, secret);
  return result;
}
static void configured_mode_and_exact_credentials() {
  Authorization a;
  TEST_ASSERT_FALSE(a.allowed(0));
  a.configure(AuthMode::none, "", "");
  TEST_ASSERT_TRUE(a.allowed(0));
  a.configure(AuthMode::pin, "", "aB!9");
  TEST_ASSERT_FALSE(a.allowed(0));
  TEST_ASSERT_EQUAL(int(AuthResult::wrong), int(a.unlock(input("", "ab!9"), 1)));
  TEST_ASSERT_EQUAL(int(AuthResult::unlocked), int(a.unlock(input("", "aB!9"), 2)));
  TEST_ASSERT_TRUE(a.allowed(2));
  a.configure(AuthMode::password, "user:name", "pass:word");
  TEST_ASSERT_EQUAL(int(AuthResult::wrong), int(a.unlock(input("user", "name:pass:word"), 3)));
  TEST_ASSERT_EQUAL(int(AuthResult::unlocked), int(a.unlock(input("user:name", "pass:word"), 4)));
  TEST_ASSERT_TRUE(a.allowed(4));
}
static void inactivity_not_motion_expires_and_input_cannot_revive() {
  Authorization a;
  a.configure(AuthMode::pin, "", "1234");
  a.unlock(input("", "1234"), 100);
  TEST_ASSERT_TRUE(a.allowed(120099));
  a.activity(120099);
  TEST_ASSERT_TRUE(a.allowed(240098));
  TEST_ASSERT_FALSE(a.allowed(240099));
  a.activity(240100);
  TEST_ASSERT_FALSE(a.allowed(240100));
  TEST_ASSERT_EQUAL(int(AuthResult::unlocked), int(a.unlock(input("", "1234"), 240101)));
  a.lock();
  TEST_ASSERT_FALSE(a.allowed(240102));
}
static void credential_change_revokes_without_resetting_attempts() {
  Authorization a;
  a.configure(AuthMode::password, "a", "b");
  a.unlock(input("a", "b"), 0);
  const auto old = a.generation();
  a.configure(AuthMode::password, "a", "b");
  TEST_ASSERT_EQUAL(old, a.generation());
  TEST_ASSERT_TRUE(a.allowed(1));
  a.configure(AuthMode::password, "a", "c");
  TEST_ASSERT_NOT_EQUAL(old, a.generation());
  TEST_ASSERT_FALSE(a.allowed(1));
  for(unsigned i=0;i<5;i++) a.unlock(input("a", "wrong"), 100+i);
  TEST_ASSERT_EQUAL(30000, a.retryAfter(104));
  a.configure(AuthMode::pin, "", "1234");
  TEST_ASSERT_EQUAL(30000, a.retryAfter(104));
  TEST_ASSERT_EQUAL(int(AuthResult::locked), int(a.unlock(input("", "1234"), 105)));
  TEST_ASSERT_FALSE(a.allowed(105));
}
static void local_attempts_are_device_wide_and_use_web_schedule() {
  Authorization a;
  a.configure(AuthMode::password, "admin", "pw");
  for(unsigned i=0;i<4;i++) {
    TEST_ASSERT_EQUAL(int(AuthResult::wrong), int(a.unlock(input("different", "wrong"), i)));
    TEST_ASSERT_EQUAL(0, a.retryAfter(i));
  }
  a.unlock(input("admin", "wrong"), 4);
  TEST_ASSERT_EQUAL(30000, a.retryAfter(4));
  TEST_ASSERT_EQUAL(int(AuthResult::locked), int(a.unlock(input("admin", "pw"), 100)));
  TEST_ASSERT_EQUAL(29904, a.retryAfter(100));
  TEST_ASSERT_EQUAL(int(AuthResult::wrong), int(a.unlock(input("admin", "wrong"), 30004)));
  TEST_ASSERT_EQUAL(60000, a.retryAfter(30004));
  TEST_ASSERT_EQUAL(int(AuthResult::unlocked), int(a.unlock(input("admin", "pw"), 90004)));
  TEST_ASSERT_EQUAL(0, a.retryAfter(90004));
  TEST_ASSERT_TRUE(a.allowed(90004));
  a.lock();
  a.unlock(input("admin", "wrong"), 90005);
  TEST_ASSERT_EQUAL(0, a.retryAfter(90005));
}
static void invalid_modes_bounds_and_large_clock_fail_closed() {
  Authorization a;
  a.configure(AuthMode::password, "\xc3\xa9", "\xf0\x9f\x94\x91");
  TEST_ASSERT_EQUAL(int(AuthResult::unlocked), int(a.unlock(input("\xc3\xa9", "\xf0\x9f\x94\x91"), UINT64_C(0x100000000))));
  TEST_ASSERT_TRUE(a.allowed(UINT64_C(0x100000001)));
  TEST_ASSERT_FALSE(a.allowed(UINT64_C(0x100000000)+120000));
  a.configure(AuthMode(99), "", "");
  TEST_ASSERT_FALSE(a.allowed(UINT64_C(0x100000001)));
  TEST_ASSERT_EQUAL(int(AuthResult::unavailable), int(a.unlock(input("", ""), 5)));
  a.configure(AuthMode::pin, "", "12345");
  TEST_ASSERT_FALSE(a.allowed(6));
  TEST_ASSERT_EQUAL(int(AuthResult::unavailable), int(a.unlock(input("", "1234"), 7)));
}
static void secret_buffers_can_be_explicitly_wiped() {
  auto c = input("sentinel-user", "sentinel-secret");
  c.clear();
  const unsigned char *p=reinterpret_cast<const unsigned char *>(&c);
  for(unsigned i=0;i<sizeof(c);i++) TEST_ASSERT_EQUAL(0, p[i]);
}
static void editor_bounds_byte_entry_and_wipe() {
  CredentialEditor e;
  e.begin(AuthMode::pin);
  e.select(33); // printable A (space is index zero)
  TEST_ASSERT_EQUAL(int(EditorResult::editing), int(e.press()));
  TEST_ASSERT_EQUAL(1, e.length());
  e.select(CredentialEditor::byteChoice);
  e.press();
  TEST_ASSERT_TRUE(e.byteMode());
  e.select(195); e.press();
  e.select(CredentialEditor::byteChoice); e.press();
  e.select(169); e.press();
  e.select(1); e.press();
  TEST_ASSERT_EQUAL(4, e.length());
  e.select(2); e.press(); // full: no append
  TEST_ASSERT_EQUAL(4, e.length());
  e.select(CredentialEditor::doneChoice);
  TEST_ASSERT_EQUAL(int(EditorResult::submitted), int(e.press()));
  Credentials out;
  TEST_ASSERT_TRUE(e.take(out));
  TEST_ASSERT_EQUAL_STRING("A\xc3\xa9!", out.secret);
  TEST_ASSERT_EQUAL(0, e.length());
  TEST_ASSERT_FALSE(e.take(out));
  out.clear();
  e.begin(AuthMode::password);
  e.select(85); e.press(); // u
  e.select(CredentialEditor::doneChoice); e.press();
  TEST_ASSERT_FALSE(e.usernameField());
  e.select(80); e.press(); // p
  e.cancel();
  TEST_ASSERT_EQUAL(0, e.length());
  TEST_ASSERT_FALSE(e.take(out));
}
static void editor_backspace_clears_tail_and_excludes_nul() {
  CredentialEditor e;
  e.begin(AuthMode::pin);
  e.select(33); e.press();
  e.select(CredentialEditor::eraseChoice); e.press();
  TEST_ASSERT_EQUAL(0, e.length());
  e.select(CredentialEditor::byteChoice); e.press();
  e.select(0);
  TEST_ASSERT_EQUAL(1, e.selected());
  e.select(300);
  TEST_ASSERT_EQUAL(255, e.selected());
  e.press();
  e.select(CredentialEditor::doneChoice); e.press();
  Credentials out;
  TEST_ASSERT_TRUE(e.take(out));
  TEST_ASSERT_EQUAL(255, uint8_t(out.secret[0]));
  TEST_ASSERT_EQUAL(0, out.secret[1]);
  for(size_t i=1;i<sizeof(out.secret);i++) TEST_ASSERT_EQUAL(0, out.secret[i]);
  out.clear();
}
static void stop_lease_freezes_only_continuing_authorized_motion() {
  StopLease lease;
  StopContext c;
  c.sessionGeneration=1; c.id=9; c.identity=123; c.members=2; c.memberIds[0]=2; c.memberIds[1]=7;
  c.memberIdentities[0]=22; c.memberIdentities[1]=77;
  lease.observe(c, 1, true, false);
  lease.observe(c, 3, false, true); // member two began after last authorized sample
  TEST_ASSERT_EQUAL(1, lease.allowed(c));
  lease.observe(c, 0, false, true);
  TEST_ASSERT_EQUAL(0, lease.allowed(c));
  lease.observe(c, 3, false, true); // idle then new movement cannot reacquire
  TEST_ASSERT_EQUAL(0, lease.allowed(c));
  lease.observe(c, 3, true, false);
  lease.observe(c, 3, false, true);
  TEST_ASSERT_EQUAL(3, lease.allowed(c));
  auto changed=c; changed.memberIdentities[1]++;
  lease.observe(changed,3,false,true);
  TEST_ASSERT_EQUAL(0,lease.allowed(c));
  lease.observe(c,3,false,true);
  TEST_ASSERT_EQUAL(0,lease.allowed(c));
  lease.observe(c,3,true,false);
  lease.observe(c,3,false,false); // explicit lock, not expiry, has no lease
  TEST_ASSERT_EQUAL(0,lease.allowed(c));
}
static void stop_lease_rejects_config_target_and_security_changes() {
  StopContext c; c.sessionGeneration=1; c.id=1; c.identity=20; c.members=1; c.memberIds[0]=1;
  c.memberIdentities[0]=20;
  for(unsigned which=0;which<5;which++) {
    StopLease lease; lease.observe(c,1,true,false); lease.observe(c,1,false,true);
    auto other=c;
    switch(which) {
    case 0: other.id++; break;
    case 1: other.identity++; break;
    case 2: other.configuration++; break;
    case 3: other.securityGeneration++; break;
    case 4: other.group=true; break;
    }
    lease.observe(other,1,false,true);
    TEST_ASSERT_EQUAL(0,lease.allowed(c));
  }
}
static void stop_grace_ends_thirty_seconds_after_original_expiry() {
  Authorization auth;
  auth.configure(AuthMode::pin,"","1234");
  const uint64_t start=UINT64_C(0x100000000);
  auth.unlock(input("","1234"),start);
  TEST_ASSERT_TRUE(auth.allowed(start+119999));
  TEST_ASSERT_FALSE(auth.allowed(start+120000));
  TEST_ASSERT_TRUE(auth.stopGraceAllowed());
  auth.activity(start+140000); // wheel/wake must not extend an expired session
  TEST_ASSERT_EQUAL(int(AuthResult::wrong),int(auth.unlock(input("","bad"),start+149999)));
  TEST_ASSERT_TRUE(auth.stopGraceAllowed());
  TEST_ASSERT_FALSE(auth.allowed(start+150000));
  TEST_ASSERT_FALSE(auth.stopGraceAllowed());
  auth.unlock(input("","bad"),start+150001);
  TEST_ASSERT_FALSE(auth.stopGraceAllowed());
  TEST_ASSERT_EQUAL(int(AuthResult::unlocked),int(auth.unlock(input("","1234"),start+150002)));
  TEST_ASSERT_TRUE(auth.allowed(start+150003));
  TEST_ASSERT_FALSE(auth.stopGraceAllowed());
}
static void delayed_poll_and_backward_clock_cannot_start_or_extend_stop_grace() {
  Authorization auth;
  auth.configure(AuthMode::pin,"","1234");
  auth.unlock(input("","1234"),1000);
  TEST_ASSERT_FALSE(auth.allowed(151000)); // first poll is already past the cap
  TEST_ASSERT_FALSE(auth.stopGraceAllowed());
  auth.unlock(input("","1234"),200000);
  TEST_ASSERT_FALSE(auth.allowed(199999));
  TEST_ASSERT_FALSE(auth.stopGraceAllowed());
}
static void clock_rollback_inside_session_or_grace_cannot_unlock_or_reset_throttle() {
  Authorization auth;
  auth.configure(AuthMode::pin,"","1234");
  auth.unlock(input("","1234"),0);
  TEST_ASSERT_TRUE(auth.allowed(60000));
  TEST_ASSERT_FALSE(auth.allowed(50000));
  TEST_ASSERT_EQUAL(int(AuthResult::locked),int(auth.unlock(input("","1234"),59999)));
  TEST_ASSERT_EQUAL(int(AuthResult::unlocked),int(auth.unlock(input("","1234"),60000)));
  TEST_ASSERT_FALSE(auth.allowed(200000));
  TEST_ASSERT_TRUE(auth.stopGraceAllowed());
  TEST_ASSERT_FALSE(auth.allowed(190000));
  TEST_ASSERT_FALSE(auth.stopGraceAllowed());
  TEST_ASSERT_EQUAL(int(AuthResult::locked),int(auth.unlock(input("","1234"),199999)));
  for(unsigned i=0;i<5;i++) auth.unlock(input("","bad"),200000+i);
  TEST_ASSERT_EQUAL(30000,auth.retryAfter(200004));
  TEST_ASSERT_GREATER_OR_EQUAL(30001,auth.retryAfter(200003));
  TEST_ASSERT_EQUAL(int(AuthResult::locked),int(auth.unlock(input("","1234"),200003)));
  TEST_ASSERT_EQUAL(30000,auth.retryAfter(200004));
}
static void grace_source_is_original_session_and_wrong_unlock_cannot_replace_it() {
  Authorization auth;auth.configure(AuthMode::pin,"","1234");
  auth.unlock(input("","1234"),0);const auto original=auth.generation();
  TEST_ASSERT_EQUAL(0,auth.expiredSessionGeneration());
  auth.allowed(120000);TEST_ASSERT_EQUAL(original,auth.expiredSessionGeneration());
  auth.unlock(input("","bad"),120001);
  TEST_ASSERT_NOT_EQUAL(original,auth.generation());
  TEST_ASSERT_EQUAL(original,auth.expiredSessionGeneration());
  auth.allowed(149999);TEST_ASSERT_EQUAL(original,auth.expiredSessionGeneration());
  auth.allowed(150000);TEST_ASSERT_EQUAL(0,auth.expiredSessionGeneration());
  auth.unlock(input("","1234"),160000);const auto replacement=auth.generation();
  TEST_ASSERT_NOT_EQUAL(original,replacement);TEST_ASSERT_EQUAL(0,auth.expiredSessionGeneration());
  auth.allowed(280000);TEST_ASSERT_EQUAL(replacement,auth.expiredSessionGeneration());
  auth.lock();TEST_ASSERT_EQUAL(0,auth.expiredSessionGeneration());
  auth.unlock(input("","1234"),300000);auth.allowed(420000);
  TEST_ASSERT_NOT_EQUAL(0,auth.expiredSessionGeneration());auth.configurationChanged();
  TEST_ASSERT_EQUAL(0,auth.expiredSessionGeneration());
  auth.unlock(input("","1234"),500000);auth.allowed(620000);auth.allowed(619999);
  TEST_ASSERT_EQUAL(0,auth.expiredSessionGeneration());
}
static void old_stop_context_cannot_borrow_an_unobserved_replacement_session() {
  Authorization auth;auth.configure(AuthMode::pin,"","1234");auth.unlock(input("","1234"),0);
  StopContext c;c.id=1;c.members=1;c.identity=20;c.memberIds[0]=1;c.memberIdentities[0]=20;
  c.sessionGeneration=auth.generation();StopLease lease;lease.observe(c,1,true,false);
  auth.allowed(120000);c.sessionGeneration=auth.expiredSessionGeneration();
  lease.observe(c,1,false,auth.stopGraceAllowed());TEST_ASSERT_EQUAL(1,lease.allowed(c));
  auth.unlock(input("","1234"),120001); // deliberately no lease observation of new active session
  auth.allowed(240001);c.sessionGeneration=auth.expiredSessionGeneration();
  lease.observe(c,1,false,auth.stopGraceAllowed());TEST_ASSERT_EQUAL(0,lease.allowed(c));
  c.sessionGeneration=0;lease.observe(c,1,true,false);lease.observe(c,1,false,true);
  TEST_ASSERT_EQUAL(0,lease.allowed(c)); // absent old aggregate source fails closed
}
static void external_idle_evidence_only_intersects_an_exact_sample_without_freezing() {
  StopContext c;c.id=9;c.identity=90;c.members=2;c.memberIds[0]=1;c.memberIds[1]=2;
  c.memberIdentities[0]=10;c.memberIdentities[1]=20;c.sessionGeneration=4;c.securityGeneration=2;c.configuration=7;
  StopLease lease;lease.retainMoving(c,3);lease.observe(c,3,false,true);TEST_ASSERT_EQUAL(0,lease.allowed(c));
  lease.observe(c,3,true,false);lease.retainMoving(c,1);TEST_ASSERT_EQUAL(0,lease.allowed(c)); // does not freeze early
  lease.observe(c,3,false,true);TEST_ASSERT_EQUAL(1,lease.allowed(c));
  auto unrelated=c;++unrelated.sessionGeneration;lease.retainMoving(unrelated,0);TEST_ASSERT_EQUAL(1,lease.allowed(c));
  unrelated=c;++unrelated.configuration;lease.retainMoving(unrelated,0);TEST_ASSERT_EQUAL(1,lease.allowed(c));
  lease.retainMoving(c,3);TEST_ASSERT_EQUAL(1,lease.allowed(c)); // never broadens or reseeds
  lease.retainMoving(c,0);lease.observe(c,3,false,true);TEST_ASSERT_EQUAL(0,lease.allowed(c));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(configured_mode_and_exact_credentials);
  RUN_TEST(inactivity_not_motion_expires_and_input_cannot_revive);
  RUN_TEST(credential_change_revokes_without_resetting_attempts);
  RUN_TEST(local_attempts_are_device_wide_and_use_web_schedule);
  RUN_TEST(invalid_modes_bounds_and_large_clock_fail_closed);
  RUN_TEST(secret_buffers_can_be_explicitly_wiped);
  RUN_TEST(editor_bounds_byte_entry_and_wipe);
  RUN_TEST(editor_backspace_clears_tail_and_excludes_nul);
  RUN_TEST(stop_lease_freezes_only_continuing_authorized_motion);
  RUN_TEST(stop_lease_rejects_config_target_and_security_changes);
  RUN_TEST(stop_grace_ends_thirty_seconds_after_original_expiry);
  RUN_TEST(delayed_poll_and_backward_clock_cannot_start_or_extend_stop_grace);
  RUN_TEST(clock_rollback_inside_session_or_grace_cannot_unlock_or_reset_throttle);
  RUN_TEST(grace_source_is_original_session_and_wrong_unlock_cannot_replace_it);
  RUN_TEST(old_stop_context_cannot_borrow_an_unobserved_replacement_session);
  RUN_TEST(external_idle_evidence_only_intersects_an_exact_sample_without_freezing);
  return UNITY_END();
}

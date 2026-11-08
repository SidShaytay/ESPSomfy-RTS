#include <unity.h>
#include "LocalAuthorization.h"
#include "LocalCredentialEditor.h"
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
  return UNITY_END();
}

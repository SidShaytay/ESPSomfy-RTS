// Production credential adapter, isolated from the real config/network/radio.
#define configsettings_h
#include "LocalUiAuthorizationAdapter.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
uint64_t authTestNow=0;
enum class security_types : uint8_t { None, PinEntry, Password };
struct ConfigSettings {
  struct { security_types type=security_types::PinEntry;
           char username[33]="", password[33]="", pin[5]="aB!9"; } Security;
} settings;
#include "../../src/LocalUiAuthorizationAdapter.cpp"
int main() {
  using namespace LocalUi;
  assert(!pollAuthorization().allowed); // first secured boot fails closed
  Credentials given;
  strcpy(given.secret,"aB!9");
  assert(unlockAuthorization(given)==AuthResult::unlocked && !given.secret[0]);
  const auto first=pollAuthorization();
  assert(first.allowed && !first.expired);
  authTestNow=119999; assert(pollAuthorization().allowed);
  authTestNow=120000; auto expired=pollAuthorization();
  assert(!expired.allowed && expired.expired);
  assert(first.generation!=expired.generation);
  assert(first.configurationGeneration==expired.configurationGeneration);
  authorizationActivity(); assert(!pollAuthorization().allowed); // input cannot revive timeout
  for(unsigned i=0;i<5;i++) assert(unlockAuthorization(given)==AuthResult::wrong);
  assert(pollAuthorization().retryAfterMs==30000 && pollAuthorization().expired);
  strcpy(given.secret,"aB!9");
  assert(unlockAuthorization(given)==AuthResult::locked && !given.secret[0]);
  authTestNow+=30000; strcpy(given.secret,"aB!9");
  assert(unlockAuthorization(given)==AuthResult::unlocked);
  strcpy(settings.Security.pin,"new!");
  assert(!pollAuthorization().allowed && !pollAuthorization().expired);
  assert(first.configurationGeneration!=pollAuthorization().configurationGeneration);
  strcpy(given.secret,"new!"); assert(unlockAuthorization(given)==AuthResult::unlocked);
  const auto beforeInactive=pollAuthorization();
  strcpy(settings.Security.password,"inactive-password"); // PIN mode still active
  const auto afterInactive=pollAuthorization();
  assert(!afterInactive.allowed && !afterInactive.expired);
  assert(afterInactive.configurationGeneration!=beforeInactive.configurationGeneration);
  strcpy(given.secret,"new!"); assert(unlockAuthorization(given)==AuthResult::unlocked);
  strcpy(settings.Security.username,"inactive-user");
  assert(!pollAuthorization().allowed);
  strcpy(given.secret,"new!"); assert(unlockAuthorization(given)==AuthResult::unlocked);
  lockAuthorization(); assert(!pollAuthorization().allowed && !pollAuthorization().expired);
  settings.Security.type=security_types::None;
  assert(pollAuthorization().allowed);
  settings.Security.type=security_types::Password;
  strcpy(settings.Security.username,"user"); strcpy(settings.Security.password,"pass");
  pollAuthorization(); strcpy(given.username,"user"); strcpy(given.secret,"pass");
  assert(unlockAuthorization(given)==AuthResult::unlocked);
  strcpy(settings.Security.pin,"old?"); // inactive PIN in password mode
  assert(!pollAuthorization().allowed);
  puts("Authorization adapter: configured credentials, secured boot, expiry, device-wide lockout, change revocation and wipe passed.");
}

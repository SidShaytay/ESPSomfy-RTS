#include "ConfigSettings.h"
#include <string.h>
bool SecuritySettings::begin() { return this->load(); }
// A device that had "Secure Config Only" keeps its PIN or password and so is
// Secured. Its old permissions key is ignored and retained for compatibility.
bool SecuritySettings::load() {
  SecurityRecord record;
  loadState=readSecurityRecord(record);
  // A failed read cannot retain a prior secret or become a None-mode login.
  volatile char *fields[]={username,password,pin};
  const size_t lengths[]={sizeof(username),sizeof(password),sizeof(pin)};
  for(size_t field=0;field<3;++field)
    for(size_t i=0;i<lengths[field];++i)fields[field][i]=0;
  type=security_types::None;
  if(!loadTrusted())return false;
  type=security_types(record.type);
  memcpy(username,record.username,sizeof(username));
  memcpy(password,record.password,sizeof(password));
  memcpy(pin,record.pin,sizeof(pin));
  return true;
}


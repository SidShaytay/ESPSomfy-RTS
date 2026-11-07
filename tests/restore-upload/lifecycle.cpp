#include <cassert>
#include <functional>
#include <map>
#include <string>
#include "RestoreUpload.h"
struct Request {
  std::map<std::string, bool> attributes;
  std::function<void()> disconnect;
  bool getAttribute(const char *key, bool fallback) { auto it=attributes.find(key); return it==attributes.end()?fallback:it->second; }
  void setAttribute(const char *key, bool value) { attributes[key]=value; }
  void onDisconnect(std::function<void()> callback) { disconnect=callback; }
};
struct Gate {
  bool owned=false; unsigned claims=0, releases=0;
  bool beginManualUpdate() { ++claims; if(owned) return false; owned=true; return true; }
  void endManualUpdate() { assert(owned); owned=false; ++releases; }
};
struct Files { unsigned removes=0; void remove(const char *) { ++removes; } };
int main() {
  Gate gate; Files files;
  Request first, other;
  assert(beginRestoreUpload(&first,gate,files));
  assert(gate.owned);
  assert(!beginRestoreUpload(&other,gate,files));
  assert(!restoreUploadComplete(&other));
  assert(files.removes==0); // a refused client cannot remove the owner's temp
  finishRestoreUpload(&first);
  assert(restoreUploadComplete(&first));
  first.disconnect();
  assert(!gate.owned && gate.releases==1 && files.removes==1);
  first.disconnect();
  assert(gate.releases==1); // cleanup is idempotent
  Request queued;
  assert(beginRestoreUpload(&queued,gate,files));
  finishRestoreUpload(&queued);
  queueRestoreUpload(&queued);
  queued.disconnect();
  assert(gate.owned && gate.releases==1); // loop owns file+gate until reboot
  gate.endManualUpdate(); // loop's failed restore release
  Request invalid;
  assert(beginRestoreUpload(&invalid,gate,files));
  discardRestoreUpload(&invalid,gate,files);
  assert(!gate.owned && !restoreUploadComplete(&invalid));
  invalid.disconnect();
  assert(gate.releases==3);
  Request multiple;
  assert(beginRestoreUpload(&multiple,gate,files));
  finishRestoreUpload(&multiple);
  assert(!beginRestoreUpload(&multiple,gate,files));
  assert(!gate.owned && !restoreUploadComplete(&multiple));
  Request truncated;
  assert(beginRestoreUpload(&truncated,gate,files));
  assert(!restoreUploadComplete(&truncated));
  truncated.disconnect();
  assert(!gate.owned);
}

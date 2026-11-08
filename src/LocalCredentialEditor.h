#pragma once
#include "LocalAuthorization.h"
namespace LocalUi {
enum class EditorResult : uint8_t { editing, submitted, cancelled };
// Only the masked length, input choice and field kind may reach the renderer.
// ASCII is convenient; Byte 01..FF can enter exact existing UTF-8 credentials.
class CredentialEditor {
  Credentials draft;
  uint16_t choice = 0;
  uint8_t limit = 0;
  bool user = false, bytes = false, pending = false;
  char *field() { return user ? draft.username : draft.secret; }
  const char *field() const { return user ? draft.username : draft.secret; }
public:
  static constexpr uint16_t eraseChoice = 95, byteChoice = 96, doneChoice = 97, cancelChoice = 98;
  ~CredentialEditor() { cancel(); }
  void begin(AuthMode mode) {
    cancel();
    user = mode == AuthMode::password;
    limit = mode == AuthMode::pin ? 4 : mode == AuthMode::password ? 32 : 0;
  }
  bool usernameField() const { return user; }
  bool byteMode() const { return bytes; }
  uint16_t selected() const { return choice; }
  uint8_t maximum() const { return limit; }
  uint8_t length() const {
    uint8_t n = 0;
    while(n < 32 && field()[n]) ++n;
    return n;
  }
  void select(int value) {
    const int low = bytes ? 1 : 0, high = bytes ? 255 : cancelChoice;
    choice = value < low ? low : value > high ? high : value;
  }
  void turn(int direction) { select(int(choice) + (direction > 0 ? 1 : -1)); }
  EditorResult press() {
    const uint8_t n = length();
    if(!bytes && choice == cancelChoice) { cancel(); return EditorResult::cancelled; }
    if(!bytes && choice == doneChoice) {
      if(user) { user = false; choice = 0; }
      else pending = true;
      return pending ? EditorResult::submitted : EditorResult::editing;
    }
    if(!bytes && choice == eraseChoice) {
      if(n) field()[n - 1] = 0;
    } else if(!bytes && choice == byteChoice) {
      bytes = true;
      choice = 1;
    } else {
      if(n < limit) field()[n] = char(bytes ? choice : choice + 32);
      if(bytes) { bytes = false; choice = byteChoice; }
    }
    return EditorResult::editing;
  }
  bool take(Credentials &out) {
    if(!pending) return false;
    out = draft;
    cancel();
    return true;
  }
  void cancel() {
    draft.clear();
    choice = 0;
    limit = 0;
    user = bytes = pending = false;
  }
};
} // namespace LocalUi

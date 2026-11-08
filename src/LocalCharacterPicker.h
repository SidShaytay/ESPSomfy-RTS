#pragma once
#include <stdint.h>
namespace LocalUi {
enum class CharacterAction : uint8_t { append, erase, mode, apply, cancel };
struct CharacterInput { CharacterAction action; uint8_t byte=0; };
// Shared ASCII plus exact-byte rotary input. No field value or domain operation
// is held here; the owner applies its own byte bound and secret-screen policy.
class CharacterPicker {
  uint16_t choice=0;
  bool bytes=false;
public:
  static constexpr uint16_t eraseChoice=95, byteChoice=96, applyChoice=97, cancelChoice=98;
  void reset() { choice=0; bytes=false; }
  bool byteMode() const { return bytes; }
  uint16_t selected() const { return choice; }
  void select(int value) {
    const int low=bytes?1:0, high=bytes?255:cancelChoice;
    choice=value<low?low:value>high?high:value;
  }
  void turn(int direction) { if(direction) select(int(choice)+(direction>0?1:-1)); }
  CharacterInput press() {
    if(bytes) {
      const uint8_t value=uint8_t(choice);
      bytes=false; choice=byteChoice;
      return {CharacterAction::append,value};
    }
    if(choice<eraseChoice) return {CharacterAction::append,uint8_t(choice+32)};
    if(choice==eraseChoice) return {CharacterAction::erase};
    if(choice==byteChoice) { bytes=true; choice=1; return {CharacterAction::mode}; }
    return {choice==applyChoice?CharacterAction::apply:CharacterAction::cancel};
  }
};
} // namespace LocalUi

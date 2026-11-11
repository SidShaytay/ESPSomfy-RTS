#pragma once
#include <stdint.h>
namespace LocalUi {
// Renderable selection metadata only; never contains typed text or credentials.
struct KeyboardView {
  uint8_t selected=0, bank=0;
  bool shift=false;
};
enum class KeyboardAction : uint8_t { none, append, erase, done };
struct KeyboardInput { KeyboardAction action=KeyboardAction::none; uint8_t byte=0; };
class RotaryKeyboard {
  KeyboardView state;
public:
  void reset(uint8_t bank=0) {state={};state.bank=bank<3?bank:0;}
  const KeyboardView &view() const {return state;}
  static const char *characters(const KeyboardView &v) {
    return v.bank==1?"0123456789-_.,:@":v.bank==2?"!\"#$%&'()*+/;<=>?[\\]^`{|}~":v.shift?"ABCDEFGHIJKLMNOPQRSTUVWXYZ":"abcdefghijklmnopqrstuvwxyz";
  }
  static uint8_t characterCount(const KeyboardView &v) {
    if(v.bank==3)return 0;
    const char *p=characters(v);uint8_t n=0;while(p[n])++n;return n;
  }
  static uint8_t actionOffset(const KeyboardView &v) {return characterCount(v)+(v.bank==2?1:0);}
  static uint16_t count(const KeyboardView &v) {return v.bank==3?255:actionOffset(v)+6;}
  static char character(const KeyboardView &v,uint8_t i) {return i<characterCount(v)?characters(v)[i]:0;}
  static const char *actionLabel(uint8_t i) {
    static const char *labels[]={"Shift","123","#+=","Space","Del","Done","Byte"};
    return i<7?labels[i]:"";
  }
  void turn(int direction) {
    if(!direction)return;
    const int total=count(state);
    state.selected=uint8_t((int(state.selected)+(direction>0?1:total-1))%total);
  }
  bool back() {
    if(state.bank!=3)return false;
    state.bank=2;state.selected=characterCount(state);return true;
  }
  KeyboardInput press() {
    if(state.bank==3) {
      const auto value=uint8_t(state.selected+1);back();
      return {KeyboardAction::append,value};
    }
    const auto letters=characterCount(state);
    if(state.selected<letters) {
      const uint8_t value=uint8_t(character(state,state.selected));
      if(state.bank==0)state.shift=false;
      return {KeyboardAction::append,value};
    }
    if(state.bank==2 && state.selected==letters){state.bank=3;state.selected=0;return {};}
    switch(state.selected-actionOffset(state)) {
      case 0:state.bank=0;state.shift=!state.shift;state.selected=0;break;
      case 1:state.bank=state.bank==1?0:1;state.selected=0;break;
      case 2:state.bank=state.bank==2?0:2;state.selected=0;break;
      case 3:return {KeyboardAction::append,' '};
      case 4:return {KeyboardAction::erase};
      case 5:return {KeyboardAction::done};

    }
    return {};
  }
};
} // namespace LocalUi

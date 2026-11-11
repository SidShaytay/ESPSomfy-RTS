#pragma once
#include "LocalCharacterPicker.h"
#include "LocalKeyboard.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
namespace LocalUi {
enum class TextEncoding : uint8_t { utf8, bytes };
enum class ValueKind : uint8_t { none, text, integer, fixed2, boolean, choice };
enum class EditOutcome : uint8_t { idle, editing, applied, cancelled, expired, invalid, invalidUtf8, full };
// One bounded local draft. The caller owns the original settings and performs
// shared domain validation/mutation only after an explicit successful take().
class ValueEditor {
  char original[65]={}, draft[65]={};
  CharacterPicker picker;
  RotaryKeyboard keyboard;
  int64_t number=0, before=0, minimum=0, maximum=0, increment=1;
  uint64_t activity=0, observed=0;
  ValueKind type=ValueKind::none;
  EditOutcome state=EditOutcome::idle;
  TextEncoding encoding=TextEncoding::utf8;
  uint8_t limit=0;
  bool editing=false, secret=false;
  static void wipe(void *memory,size_t bytes) {
    auto *out=static_cast<volatile uint8_t *>(memory);
    while(bytes--) *out++=0;
  }
  void close(EditOutcome result) {
    wipe(original,sizeof(original)); wipe(draft,sizeof(draft)); picker.reset(); keyboard.reset();
    number=before=minimum=maximum=activity=observed=limit=0; increment=1;
    type=ValueKind::none; editing=secret=false; state=result;
  }
  static bool utf8(const char *text) {
    const auto *p=reinterpret_cast<const uint8_t *>(text);
    while(*p) {
      uint32_t code=*p++; unsigned count=0;
      if(code<0x80) continue;
      uint32_t least;
      if(code>=0xc2 && code<=0xdf) { count=1; code&=0x1f; least=0x80; }
      else if(code>=0xe0 && code<=0xef) { count=2; code&=0x0f; least=0x800; }
      else if(code>=0xf0 && code<=0xf4) { count=3; code&=0x07; least=0x10000; }
      else return false;
      while(count--) {
        if((*p&0xc0)!=0x80) return false;
        code=(code<<6)|(*p++&0x3f);
      }
      if(code<least || code>0x10ffff || (code>=0xd800 && code<=0xdfff)) return false;
    }
    return true;
  }
  bool beginNumber(ValueKind kind,int64_t value,int64_t low,int64_t high,int64_t step,uint64_t now) {
    close(EditOutcome::invalid);
    if(low>high || value<low || value>high || step<=0) return false;
    type=kind; number=before=value; minimum=low; maximum=high; increment=step;
    activity=observed=now; editing=true; state=EditOutcome::editing; return true;
  }
public:
  static constexpr uint64_t timeoutMs=30000;
  ValueEditor()=default;
  ValueEditor(const ValueEditor &)=delete;
  ValueEditor &operator=(const ValueEditor &)=delete;
  ~ValueEditor() { close(EditOutcome::idle); }
  // Read-only validation for callers offering an explicit invalid-value
  // replacement flow. beginText retains its legacy-preservation contract.
  static bool validUtf8(const char *text) { return text && utf8(text); }
  ValueKind kind() const { return type; }
  EditOutcome outcome() const { return state; }
  bool active() const { return editing; }
  uint8_t length() const { return uint8_t(strlen(draft)); }
  uint8_t maximumBytes() const { return limit; }
  bool byteMode() const { return picker.byteMode(); }
  bool masked() const { return secret; }
  uint16_t selected() const { return picker.selected(); }
  bool dirty() const { return type==ValueKind::text?memcmp(original,draft,sizeof(draft))!=0:number!=before; }
  bool beginText(const char *value,size_t maxBytes,TextEncoding policy,bool masked,uint64_t now) {
    close(EditOutcome::invalid);
    if(!value || maxBytes>64 || (policy!=TextEncoding::utf8 && policy!=TextEncoding::bytes)) return false;
    size_t count=0; while(count<=maxBytes && value[count]) ++count;
    if(count>maxBytes) return false;
    memcpy(original,value,count); memcpy(draft,value,count);
    limit=uint8_t(maxBytes); encoding=policy; secret=masked; activity=observed=now;
    type=ValueKind::text; editing=true; state=EditOutcome::editing; return true;
  }
  bool beginInteger(int64_t value,int64_t low,int64_t high,int64_t step,uint64_t now) {
    return beginNumber(ValueKind::integer,value,low,high,step,now);
  }
  bool beginFixed2(double value,int64_t lowHundredths,int64_t highHundredths,int64_t stepHundredths,uint64_t now) {
    close(EditOutcome::invalid);
    if(!isfinite(value) || lowHundredths>highHundredths || stepHundredths<=0) return false;
    const long double scaled=roundl(static_cast<long double>(value)*100);
    if(!isfinite(scaled) || scaled>=9223372036854775808.0L || scaled< -9223372036854775808.0L ||
       scaled<static_cast<long double>(lowHundredths) ||
       scaled>static_cast<long double>(highHundredths)) return false;
    return beginNumber(ValueKind::fixed2,int64_t(scaled),lowHundredths,highHundredths,stepHundredths,now);
  }
  bool beginBoolean(bool value,uint64_t now) { return beginNumber(ValueKind::boolean,value,0,1,1,now); }
  bool beginChoice(uint8_t index,uint8_t count,uint64_t now) {
    if(!count || index>=count) { close(EditOutcome::invalid); return false; }
    return beginNumber(ValueKind::choice,index,0,count-1,1,now);
  }
  void tick(uint64_t now) {
    if(type==ValueKind::none) return;
    if(now<observed || now<activity || now-activity>=timeoutMs) close(EditOutcome::expired);
    else observed=now;
  }
  void select(int choice,uint64_t now) {
    tick(now); if(!editing || type!=ValueKind::text) return;
    picker.select(choice); activity=now; state=EditOutcome::editing;
  }
  // Changing precision only changes future rotation, never applies the draft.
  bool setStep(int64_t step,uint64_t now) {
    tick(now);
    if(!editing || (type!=ValueKind::integer && type!=ValueKind::fixed2) || step<=0) return false;
    increment=step; activity=now; return true;
  }
  void rotate(int direction,uint64_t now) {
    tick(now); if(!editing || !direction) return;
    activity=now; state=EditOutcome::editing;
    if(type==ValueKind::text) { picker.turn(direction); return; }
    if(direction>0) {
      const uint64_t room=uint64_t(maximum)-uint64_t(number);
      number=uint64_t(increment)>=room?maximum:number+increment;
    } else {
      const uint64_t room=uint64_t(number)-uint64_t(minimum);
      number=uint64_t(increment)>=room?minimum:number-increment;
    }
  }
  EditOutcome press(uint64_t now) {
    tick(now); if(!editing) return state;
    if(type!=ValueKind::text) return apply(now);
    activity=now; state=EditOutcome::editing;
    const auto input=picker.press();
    if(input.action==CharacterAction::apply) return apply(now);
    if(input.action==CharacterAction::cancel) { cancel(); return state; }
    const uint8_t count=length();
    if(input.action==CharacterAction::erase) { if(count) draft[count-1]=0; }
    else if(input.action==CharacterAction::append) {
      if(count==limit) state=EditOutcome::full;
      else draft[count]=char(input.byte);
    }
    return state;
  }
  const KeyboardView &keyboardView() const {return keyboard.view();}
  void keyboardTurn(int direction,uint64_t now) {
    tick(now);if(!editing || type!=ValueKind::text || !direction)return;
    keyboard.turn(direction);activity=now;state=EditOutcome::editing;
  }
  bool keyboardBack(uint64_t now) {
    tick(now);if(!editing || type!=ValueKind::text)return false;
    const bool back=keyboard.back();if(back)activity=now;return back;
  }
  EditOutcome keyboardPress(uint64_t now) {
    tick(now);if(!editing || type!=ValueKind::text)return state;
    activity=now;state=EditOutcome::editing;
    const auto input=keyboard.press();
    if(input.action==KeyboardAction::done)return apply(now);
    const uint8_t count=length();
    if(input.action==KeyboardAction::erase && count) {
      uint8_t start=count-1;
      if(encoding==TextEncoding::utf8 && utf8(draft))
        while(start && (uint8_t(draft[start])&0xc0)==0x80)--start;
      wipe(draft+start,sizeof(draft)-start);
    } else if(input.action==KeyboardAction::append) {
      if(count==limit)state=EditOutcome::full;
      else {draft[count]=char(input.byte);draft[count+1]=0;}
    }
    return state;
  }
  EditOutcome apply(uint64_t now) {
    tick(now); if(!editing) return state;
    activity=now;
    if(type==ValueKind::text && dirty() && encoding==TextEncoding::utf8 && !utf8(draft))
      return state=EditOutcome::invalidUtf8;
    editing=false; return state=EditOutcome::applied;
  }
  void cancel() { close(EditOutcome::cancelled); }
  void preview(char *out,size_t capacity) const {
    if(!out || !capacity) return;
    out[0]=0;
    if(type==ValueKind::text) {
      size_t count=length(); if(count>=capacity) count=capacity-1;
      if(secret) memset(out,'*',count);
      else {
        if(encoding==TextEncoding::utf8 && utf8(draft))
          while(count && (uint8_t(draft[count])&0xc0)==0x80) --count;
        memcpy(out,draft,count);
      }
      out[count]=0;
    } else if(type==ValueKind::fixed2) {
      const uint64_t magnitude=number<0?uint64_t(-(number+1))+1:uint64_t(number);
      snprintf(out,capacity,"%s%llu.%02llu",number<0?"-":"",(unsigned long long)(magnitude/100),
               (unsigned long long)(magnitude%100));
    } else if(type==ValueKind::boolean) snprintf(out,capacity,"%s",number?"On":"Off");
    else if(type!=ValueKind::none) snprintf(out,capacity,"%lld",(long long)number);
  }
  // Keep newly entered bytes visible. Ellipsis signals a clipped prefix;
  // secrets expose only masks. Valid UTF-8 tails never begin mid-codepoint.
  void previewTail(char *out,size_t capacity) const {
    if(type!=ValueKind::text || length()<capacity || capacity<=4) {
      preview(out,capacity); return;
    }
    if(!out) return;
    const size_t count=length(); size_t start=count-(capacity-4);
    if(!secret && utf8(draft))
      while(start<count && (uint8_t(draft[start])&0xc0)==0x80) ++start;
    memcpy(out,"...",3);
    if(secret) memset(out+3,'*',count-start);
    else memcpy(out+3,draft+start,count-start);
    out[3+count-start]=0;
  }
  bool takeText(char *out,size_t capacity) {
    if(state!=EditOutcome::applied || type!=ValueKind::text || !out || capacity<=length()) return false;
    memcpy(out,draft,length()+1); close(EditOutcome::idle); return true;
  }
  bool takeNumber(int64_t &out) {
    if(state!=EditOutcome::applied || (type!=ValueKind::integer && type!=ValueKind::fixed2)) return false;
    out=number; close(EditOutcome::idle); return true;
  }
  bool takeChoice(uint8_t &out) {
    if(state!=EditOutcome::applied || (type!=ValueKind::choice && type!=ValueKind::boolean)) return false;
    out=uint8_t(number); close(EditOutcome::idle); return true;
  }
};
static_assert(sizeof(ValueEditor)<=256,"One LCD draft must remain bounded to 256 bytes");
} // namespace LocalUi

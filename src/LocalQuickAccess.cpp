#include "LocalQuickAccess.h"
#include <string.h>
namespace LocalUi {
namespace {
bool validBinding(EntityRef ref,uint32_t identity) {return ref.valid() && identity && identity<=0xFFFFFF;}
uint32_t crc32(const uint8_t *bytes,size_t size) {
  uint32_t value=0xFFFFFFFFu;
  for(size_t i=0;i<size;++i) {
    value^=bytes[i];
    for(unsigned bit=0;bit<8;++bit)value=(value>>1)^((value&1)?0xEDB88320u:0u);
  }
  return ~value;
}
}
QuickAccessEdit QuickAccess::assign(size_t slot,EntityRef ref,uint32_t identity) {
  if(slot>=quickAccessSlots)return QuickAccessEdit::invalidSlot;
  if(!validBinding(ref,identity))return QuickAccessEdit::invalidBinding;
  for(size_t i=0;i<quickAccessSlots;++i)
    if(i!=slot && !entries[i].empty() && entries[i].ref==ref)return QuickAccessEdit::duplicate;
  if(entries[slot].ref==ref && entries[slot].identity==identity)return QuickAccessEdit::unchanged;
  entries[slot]={ref,identity};return QuickAccessEdit::changed;
}
QuickAccessEdit QuickAccess::remove(size_t slot) {
  if(slot>=quickAccessSlots)return QuickAccessEdit::invalidSlot;
  if(entries[slot].empty())return QuickAccessEdit::unchanged;
  entries[slot]={};return QuickAccessEdit::changed;
}
QuickAccessEdit QuickAccess::move(size_t from,size_t to) {
  if(from>=quickAccessSlots || to>=quickAccessSlots)return QuickAccessEdit::invalidSlot;
  if(from==to)return QuickAccessEdit::unchanged;
  const auto selected=entries[from];
  if(from<to)for(size_t i=from;i<to;++i)entries[i]=entries[i+1];
  else for(size_t i=from;i>to;--i)entries[i]=entries[i-1];
  entries[to]=selected;return QuickAccessEdit::changed;
}
bool encodeQuickAccess(const QuickAccess &value,uint8_t *out,size_t capacity) {
  if(!out || capacity<quickAccessBytes)return false;
  uint8_t bytes[quickAccessBytes]={'L','Q','A',1};
  for(size_t i=0;i<quickAccessSlots;++i) {
    const auto &entry=*value.at(i);if(entry.empty())continue;
    if(!validBinding(entry.ref,entry.identity))return false;
    auto *slot=bytes+4+i*8;
    slot[0]=entry.ref.kind==EntityKind::device?1:2;slot[1]=entry.ref.id;
    slot[2]=uint8_t(entry.identity>>16);slot[3]=uint8_t(entry.identity>>8);slot[4]=uint8_t(entry.identity);
  }
  const uint32_t crc=crc32(bytes,28);
  for(unsigned i=0;i<4;++i)bytes[28+i]=uint8_t(crc>>(i*8));
  memcpy(out,bytes,sizeof(bytes));return true;
}
QuickAccessLoad decodeQuickAccess(const uint8_t *bytes,size_t size,QuickAccess &out) {
  out={};
  if(!bytes || size!=quickAccessBytes || memcmp(bytes,"LQA",3) || bytes[3]!=1)return QuickAccessLoad::invalid;
  uint32_t expected=0;for(unsigned i=0;i<4;++i)expected|=uint32_t(bytes[28+i])<<(i*8);
  if(crc32(bytes,28)!=expected)return QuickAccessLoad::invalid;
  QuickAccess candidate;
  for(size_t i=0;i<quickAccessSlots;++i) {
    const auto *slot=bytes+4+i*8;
    if(slot[5] || slot[6] || slot[7])return QuickAccessLoad::invalid;
    if(!slot[0]) {
      if(slot[1] || slot[2] || slot[3] || slot[4])return QuickAccessLoad::invalid;
      continue;
    }
    if(slot[0]!=1 && slot[0]!=2)return QuickAccessLoad::invalid;
    const EntityRef ref{slot[0]==1?EntityKind::device:EntityKind::group,slot[1]};
    const uint32_t identity=(uint32_t(slot[2])<<16)|(uint32_t(slot[3])<<8)|slot[4];
    if(candidate.assign(i,ref,identity)!=QuickAccessEdit::changed)return QuickAccessLoad::invalid;
  }
  out=candidate;return QuickAccessLoad::valid;
}
QuickAccessLoad loadQuickAccess(const QuickAccessRead &read,QuickAccess &out) {
  out={};
  switch(read.state) {
    case QuickAccessReadState::present:return decodeQuickAccess(read.bytes,read.size,out);
    case QuickAccessReadState::missing:return QuickAccessLoad::missing;
    case QuickAccessReadState::unreadable:return QuickAccessLoad::unreadable;
  }
  return QuickAccessLoad::invalid;
}
} // namespace LocalUi

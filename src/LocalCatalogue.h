#pragma once
#include "ShadeTypes.h"
#include "LocalText.h"
#include <string.h>

namespace LocalUi {
enum class CatalogueKind : uint8_t { devices, groups, rooms, roomContents };
struct CatalogueEntry {
  uint8_t id = 0, roomId = 0;
  int8_t sortOrder = 0;
  char name[21] = {};
  uint32_t identity = 0; // Guard metadata only; never a label or an index-derived RF target.
  uint64_t revision = 0;
  int8_t position = -1;
  bool moving = false, available = false;
  bool group = false, ambiguous = false;
};
// A display fingerprint is deliberately independent of IDs and model text.
// Unsupported UTF-8 glyphs collapse exactly as they do in the bundled font.
inline uint32_t lcdNameHash(const char *name) {
  char visible[21];lcdLabel(name,visible,sizeof(visible));
  uint32_t hash=2166136261u;
  for(const char *p=visible;*p;++p)hash=(hash^uint8_t(*p))*16777619u;
  return hash;
}
inline void markAmbiguousRows(CatalogueEntry *rows,uint8_t count) {
  for(uint8_t i=0;i<count;++i)rows[i].ambiguous=false;
  for(uint8_t i=0;i<count;++i)for(uint8_t j=0;j<i;++j) {
    if(!rows[i].id || !rows[j].id)continue;
    char left[21],right[21];lcdLabel(rows[i].name,left,sizeof(left));lcdLabel(rows[j].name,right,sizeof(right));
    if(!strcmp(left,right))rows[i].ambiguous=rows[j].ambiguous=true;
  }
}
constexpr uint8_t catalogueRows = 4, allRooms = 255;
struct CataloguePage {
  CatalogueEntry rows[catalogueRows];
  uint8_t offset = 0, cursor = 0, count = 0, total = 0;
};
// Called only on the model-owning loop. read() copies metadata without sending
// commands or changing the model; that model must remain stable for this call.
class CatalogueSource {
public:
  virtual ~CatalogueSource() = default;
  virtual bool read(CatalogueKind kind, uint8_t slot, CatalogueEntry &out) = 0;
};

// A page carries stable model IDs, never RF identities derived from row offsets.
// Keep only small sort keys for the configured maximum and copy four labels.
inline bool cataloguePage(CatalogueSource &source, CatalogueKind kind, uint8_t offset,
                          uint8_t room, CataloguePage &out, uint8_t anchorId=0, bool anchored=false, bool anchorGroup=false) {
  out = {};
  uint8_t capacity;
  switch(kind) {
  case CatalogueKind::devices: capacity = SOMFY_MAX_SHADES; break;
  case CatalogueKind::groups: capacity = SOMFY_MAX_GROUPS; break;
  case CatalogueKind::roomContents: capacity = SOMFY_MAX_SHADES+SOMFY_MAX_GROUPS; break;
  case CatalogueKind::rooms:
    if(room != allRooms) return false;
    capacity = SOMFY_MAX_ROOMS;
    break;
  default: return false;
  }
  struct Key { uint64_t revision; uint32_t identity; uint8_t slot, id; int8_t order; bool group; };
  constexpr uint8_t maximum = SOMFY_MAX_SHADES+SOMFY_MAX_GROUPS;
  static_assert(SOMFY_MAX_GROUPS <= maximum && SOMFY_MAX_ROOMS <= maximum,
                "catalogue sort storage must cover all configured maxima");
  static_assert(maximum<=64,"Catalogue display flags fit one bounded bitmap");
  Key keys[maximum] = {};
  uint32_t displayNames[maximum]={}; // 192 bytes at the current 48-entry limit.
  uint64_t filtered=0,ambiguous=0;
  uint16_t ids[maximum] = {}; uint8_t seen = 0, count = 0;
  const auto read=[&](uint8_t slot,CatalogueEntry &entry) {
    const bool group=kind==CatalogueKind::groups || (kind==CatalogueKind::roomContents && slot>=SOMFY_MAX_SHADES);
    const auto actual=kind==CatalogueKind::roomContents?(group?CatalogueKind::groups:CatalogueKind::devices):kind;
    const uint8_t index=kind==CatalogueKind::roomContents && group?slot-SOMFY_MAX_SHADES:slot;
    if(!source.read(actual,index,entry))return false;
    entry.group=group;return true;
  };
  for(uint8_t slot = 0; slot < capacity; ++slot) {
    CatalogueEntry entry;
    if(!read(slot, entry)) continue;
    if(!entry.id || entry.id == 255 || !memchr(entry.name, 0, sizeof(entry.name))) return false;
    for(uint8_t i = 0; i < seen; ++i)
      if(ids[i] == uint16_t(entry.id|(entry.group?256:0))) return false;
    ids[seen++] = uint16_t(entry.id|(entry.group?256:0));
    if(kind != CatalogueKind::rooms && room != allRooms && entry.roomId != room) continue;
    const uint32_t display=lcdNameHash(entry.name);
    for(uint8_t other=0;other<slot;++other)
      if((filtered&(uint64_t(1)<<other)) && displayNames[other]==display)
        ambiguous|=(uint64_t(1)<<slot)|(uint64_t(1)<<other);
    filtered|=uint64_t(1)<<slot;displayNames[slot]=display;
    uint8_t at = count;
    while(at && (keys[at - 1].order > entry.sortOrder ||
          (keys[at - 1].order == entry.sortOrder &&
           (keys[at - 1].group>entry.group || (keys[at - 1].group==entry.group && keys[at - 1].id>entry.id))))) {
      keys[at] = keys[at - 1];
      --at;
    }
    keys[at] = {entry.revision,entry.identity,slot, entry.id, entry.sortOrder,entry.group};
    ++count;
  }
  CataloguePage page;
  page.total = count;
  if(anchored)
    for(uint8_t i=0;i<count;i++) if(keys[i].id==anchorId && (kind!=CatalogueKind::roomContents || keys[i].group==anchorGroup)) { offset=i; break; }
  page.cursor=count?(offset<count?offset:uint8_t(count-1)):0;
  page.offset=page.cursor/catalogueRows*catalogueRows;
  for(uint8_t i = page.offset; i < count && page.count < catalogueRows; ++i) {
    CatalogueEntry entry;
    if(!read(keys[i].slot, entry) || entry.id != keys[i].id ||
       entry.sortOrder != keys[i].order || entry.group!=keys[i].group ||
       entry.identity!=keys[i].identity || entry.revision!=keys[i].revision || !memchr(entry.name, 0, sizeof(entry.name)) ||
       (kind != CatalogueKind::rooms && room != allRooms && entry.roomId != room)) return false;
    if(lcdNameHash(entry.name)!=displayNames[keys[i].slot])return false;
    entry.ambiguous=false;
    if(ambiguous&(uint64_t(1)<<keys[i].slot)) {
      char visible[21];lcdLabel(entry.name,visible,sizeof(visible));
      for(uint8_t other=0;other<count;++other) {
        const auto &candidate=keys[other];
        if(candidate.slot==keys[i].slot || displayNames[candidate.slot]!=displayNames[keys[i].slot])continue;
        CatalogueEntry peer;
        if(!read(candidate.slot,peer) || peer.id!=candidate.id || peer.group!=candidate.group ||
           peer.identity!=candidate.identity || peer.revision!=candidate.revision || !memchr(peer.name,0,sizeof(peer.name)))return false;
        char peerVisible[21];lcdLabel(peer.name,peerVisible,sizeof(peerVisible));
        if(!strcmp(visible,peerVisible)){entry.ambiguous=true;break;}
      }
    }
    page.rows[page.count++] = entry;
  }
  out = page;
  return true;
}
// Unassigned appears only when it has contents. Global lists replace All rooms.
inline bool roomCataloguePage(CatalogueSource &source,uint8_t cursor,CataloguePage &out,
                              uint8_t anchorId=0,bool anchored=false) {
  out={}; CataloguePage stored,unassigned;
  if(!cataloguePage(source,CatalogueKind::rooms,0,allRooms,stored) ||
     !cataloguePage(source,CatalogueKind::roomContents,0,0,unassigned))return false;
  const uint8_t extra=unassigned.total?1:0;
  bool unassignedCollision=false;
  if(extra)for(uint8_t slot=0;slot<SOMFY_MAX_ROOMS;++slot) {
    CatalogueEntry entry;if(!source.read(CatalogueKind::rooms,slot,entry))continue;
    if(!memchr(entry.name,0,sizeof(entry.name)))return false;
    char visible[21];lcdLabel(entry.name,visible,sizeof(visible));
    if(!strcmp(visible,"Unassigned"))unassignedCollision=true;
  }
  if(anchored) {
    if(anchorId==0 && extra)cursor=0;
    else if(anchorId!=allRooms) {
      CataloguePage found;
      if(!cataloguePage(source,CatalogueKind::rooms,0,allRooms,found,anchorId,true))return false;
      if(found.count && found.rows[found.cursor-found.offset].id==anchorId)cursor=found.cursor+extra;
    }
  }
  CataloguePage page;page.total=stored.total+extra;
  page.cursor=page.total?(cursor<page.total?cursor:page.total-1):0;
  page.offset=page.cursor/catalogueRows*catalogueRows;
  for(uint8_t at=page.offset;at<page.total && page.count<catalogueRows;++at) {
    CatalogueEntry entry;
    if(extra && !at){strcpy(entry.name,"Unassigned");entry.ambiguous=unassignedCollision;}
    else {
      const uint8_t actual=at-extra;
      if(actual<stored.offset || actual>=stored.offset+stored.count)
        if(!cataloguePage(source,CatalogueKind::rooms,actual,allRooms,stored))return false;
      entry=stored.rows[actual-stored.offset];
      if(extra) {char visible[21];lcdLabel(entry.name,visible,sizeof(visible));if(!strcmp(visible,"Unassigned"))entry.ambiguous=true;}
    }
    page.rows[page.count++]=entry;
  }
  out=page;return true;
}
} // namespace LocalUi

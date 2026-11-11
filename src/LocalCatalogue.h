#pragma once
#include "ShadeTypes.h"
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
  bool group = false;
};
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
  Key keys[maximum] = {};
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
    if(extra && !at)strcpy(entry.name,"Unassigned");
    else {
      const uint8_t actual=at-extra;
      if(actual<stored.offset || actual>=stored.offset+stored.count)
        if(!cataloguePage(source,CatalogueKind::rooms,actual,allRooms,stored))return false;
      entry=stored.rows[actual-stored.offset];
    }
    page.rows[page.count++]=entry;
  }
  out=page;return true;
}
} // namespace LocalUi

#pragma once
#include "ShadeTypes.h"
#include <string.h>

namespace LocalUi {
enum class CatalogueKind : uint8_t { devices, groups, rooms };
struct CatalogueEntry {
  uint8_t id = 0, roomId = 0;
  int8_t sortOrder = 0;
  char name[21] = {};
  uint32_t identity = 0; // Guard metadata only; never a label or an index-derived RF target.
  uint64_t revision = 0;
  int8_t position = -1;
  bool moving = false, available = false;
};
constexpr uint8_t catalogueRows = 3, allRooms = 255;
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
// Keep only small sort keys for the configured maximum and copy three labels.
inline bool cataloguePage(CatalogueSource &source, CatalogueKind kind, uint8_t offset,
                          uint8_t room, CataloguePage &out, uint8_t anchorId=0, bool anchored=false) {
  out = {};
  uint8_t capacity;
  switch(kind) {
  case CatalogueKind::devices: capacity = SOMFY_MAX_SHADES; break;
  case CatalogueKind::groups: capacity = SOMFY_MAX_GROUPS; break;
  case CatalogueKind::rooms:
    if(room != allRooms) return false;
    capacity = SOMFY_MAX_ROOMS;
    break;
  default: return false;
  }
  struct Key { uint8_t slot, id; int8_t order; };
  constexpr uint8_t maximum = SOMFY_MAX_SHADES;
  static_assert(SOMFY_MAX_GROUPS <= maximum && SOMFY_MAX_ROOMS <= maximum,
                "catalogue sort storage must cover all configured maxima");
  Key keys[maximum] = {};
  uint8_t ids[maximum] = {}, seen = 0, count = 0;
  for(uint8_t slot = 0; slot < capacity; ++slot) {
    CatalogueEntry entry;
    if(!source.read(kind, slot, entry)) continue;
    if(!entry.id || entry.id == 255 || !memchr(entry.name, 0, sizeof(entry.name))) return false;
    for(uint8_t i = 0; i < seen; ++i)
      if(ids[i] == entry.id) return false;
    ids[seen++] = entry.id;
    if(kind != CatalogueKind::rooms && room != allRooms && entry.roomId != room) continue;
    uint8_t at = count;
    while(at && (keys[at - 1].order > entry.sortOrder ||
          (keys[at - 1].order == entry.sortOrder && keys[at - 1].id > entry.id))) {
      keys[at] = keys[at - 1];
      --at;
    }
    keys[at] = {slot, entry.id, entry.sortOrder};
    ++count;
  }
  CataloguePage page;
  page.total = count;
  if(anchored)
    for(uint8_t i=0;i<count;i++) if(keys[i].id==anchorId) { offset=i; break; }
  page.cursor=count?(offset<count?offset:uint8_t(count-1)):0;
  page.offset=page.cursor/catalogueRows*catalogueRows;
  for(uint8_t i = page.offset; i < count && page.count < catalogueRows; ++i) {
    CatalogueEntry entry;
    if(!source.read(kind, keys[i].slot, entry) || entry.id != keys[i].id ||
       entry.sortOrder != keys[i].order || !memchr(entry.name, 0, sizeof(entry.name)) ||
       (kind != CatalogueKind::rooms && room != allRooms && entry.roomId != room)) return false;
    page.rows[page.count++] = entry;
  }
  out = page;
  return true;
}
// The two virtual room filters consume no stored room slots. They never become
// a device/group EntityRef and are never passed to a command dispatcher.
inline bool roomCataloguePage(CatalogueSource &source,uint8_t cursor,CataloguePage &out,
                              uint8_t anchorId=0,bool anchored=false) {
  out={}; CataloguePage stored;
  if(!cataloguePage(source,CatalogueKind::rooms,0,allRooms,stored)) return false;
  if(anchored) {
    if(anchorId==allRooms) cursor=0;
    else if(anchorId==0) cursor=1;
    else {
      CataloguePage found;
      if(!cataloguePage(source,CatalogueKind::rooms,0,allRooms,found,anchorId,true)) return false;
      if(found.count && found.rows[found.cursor-found.offset].id==anchorId) cursor=found.cursor+2;
    }
  }
  CataloguePage page;
  page.total=stored.total+2; page.cursor=cursor<page.total?cursor:page.total-1;
  page.offset=page.cursor/catalogueRows*catalogueRows;
  if(page.offset>=2 && !cataloguePage(source,CatalogueKind::rooms,page.offset-2,allRooms,stored)) return false;
  for(uint8_t at=page.offset;at<page.total && page.count<catalogueRows;at++) {
    CatalogueEntry entry;
    if(at<2) { entry.id=at==0?allRooms:0; strcpy(entry.name,at==0?"All rooms":"Unassigned"); }
    else {
      const uint8_t actual=at-2;
      if(actual<stored.offset || actual>=stored.offset+stored.count)
        if(!cataloguePage(source,CatalogueKind::rooms,actual,allRooms,stored)) return false;
      entry=stored.rows[actual-stored.offset];
    }
    page.rows[page.count++]=entry;
  }
  out=page; return true;
}
} // namespace LocalUi

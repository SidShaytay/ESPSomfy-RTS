#include "LocalAdministration.h"
#include <stdio.h>
#include <string.h>
#include <type_traits>
namespace LocalUi {
using EntityAdmin::Kind;
using EntityAdmin::Mode;
using AdminNotice=EntityAdmin::Notice;
namespace {
constexpr Kind kinds[] = {Kind::room,Kind::shade,Kind::group};
constexpr CatalogueKind catalogues[] = {CatalogueKind::rooms,CatalogueKind::devices,CatalogueKind::groups};
constexpr const char *titles[] = {"Rooms","Devices","Groups","More"};
constexpr const char *addLabels[] = {"Add room","Add device","Add group"};
constexpr uint8_t capacities[] = {SOMFY_MAX_ROOMS,SOMFY_MAX_SHADES,SOMFY_MAX_GROUPS};
constexpr uint8_t moreType=sizeof(kinds)/sizeof(*kinds);
constexpr uint8_t typeCount=moreType+1;
}
bool Administration::takePrivateProjectionDiscard() {
  if(!advanced.takePrivateProjectionDiscard())return false;
  const auto notice=frame.notice;
  static_assert(std::is_trivially_copyable<EntityAdmin::FormView>::value,"Plain projected fields only");
  auto *bytes=reinterpret_cast<volatile unsigned char *>(&frame);
  for(size_t i=0;i<sizeof(frame);++i)bytes[i]=0;
  frame.protectedFrame=true;frame.notice=notice;
  // Never read the erased Session. Rebuild only from the active current page;
  // a closed administration retains its sanitized completion notice instead.
  if(active)render();
  return true;
}
void Administration::close(AdminNotice notice) {
  form.discard();ordering.discard();advanced.discard();frame={};frame.notice=notice;admitted={};
  revision=observedAt=0;cursor=anchor=returnId=typeCursor=0;active=false;page=Page::types;
}
bool Administration::open(uint64_t now) {
  discard();admitted=backend.authorization();revision=backend.revision();observedAt=now;
  if(!admitted.allowed || !admitted.generation || !admitted.configurationGeneration) {
    close(AdminNotice::locked);return false;
  }
  if(!revision) {close(AdminNotice::changed);return false;}
  active=true;render();return active;
}
bool Administration::entry(uint8_t ordinal,CatalogueEntry &out) {
  CataloguePage data;
  if(!cataloguePage(catalogue,catalogues[typeCursor],ordinal,allRooms,data) || ordinal>=data.total)
    return false;
  out=data.rows[ordinal-data.offset];return true;
}
void Administration::render() {
  if(page==Page::entity) {frame=form.view();return;}
  if(page==Page::order) {ordering.render(frame);return;}
  if(page==Page::advanced) {advanced.render(frame);return;}
  const auto notice=frame.notice;frame={};frame.notice=notice;
  frame.mode=Mode::fields;
  if(page==Page::types) {
    strcpy(frame.title,"Administration");strcpy(frame.hint,"Press: manage   Back: Home");
    frame.total=typeCount;frame.selected=typeCursor;frame.offset=typeCursor/3*3;
    for(uint8_t i=frame.offset;i<typeCount && frame.rowCount<3;i++) {auto &row=frame.rows[frame.rowCount++];strcpy(row.label,titles[i]);row.selected=i==typeCursor;}
    return;
  }
  if(page==Page::more) {strcpy(frame.title,"More");strcpy(frame.hint,"Press: choose   Back: types");frame.total=frame.rowCount=1;strcpy(frame.rows[0].label,"Repeaters");frame.rows[0].selected=true;return;}
  if(typeCursor>=moreType) {close(AdminNotice::changed);return;}
  frame.kind=kinds[typeCursor];
  CataloguePage data;
  if(!cataloguePage(catalogue,catalogues[typeCursor],0,allRooms,data)) {close(AdminNotice::changed);return;}
  frame.total=data.total+(typeCursor?3:2);if(cursor>=frame.total)cursor=0;
  frame.selected=cursor;frame.offset=cursor/3*3;
  strcpy(frame.title,titles[typeCursor]);strcpy(frame.hint,"Press: choose   Back: types");
  anchor=0;
  for(uint8_t i=frame.offset;i<frame.total && frame.rowCount<3;i++) {
    auto &row=frame.rows[frame.rowCount++];row.selected=i==cursor;
    if(!i) snprintf(row.label,sizeof(row.label),"%s%s",addLabels[typeCursor],data.total>=capacities[typeCursor]?" (full)":"");
    else if(i==data.total+1) strcpy(row.label,"Reorder");
    else if(typeCursor && i==data.total+2)strcpy(row.label,"Advanced");
    else {
      CatalogueEntry item;if(!entry(i-1,item)) {close(AdminNotice::changed);return;}
      snprintf(row.label,sizeof(row.label),"#%u %s",unsigned(item.id),item.name);
      if(row.selected)frame.id=anchor=item.id;
    }
  }
}
bool Administration::guard(uint64_t now) {
  if(!active)return false;
  if(now<observedAt) {close(AdminNotice::expired);return false;}
  observedAt=now;
  if(page==Page::entity || page==Page::order || page==Page::advanced)return true; // Each child owns its captured history/result.
  const auto current=backend.authorization();
  if(!current.allowed) {close(AdminNotice::locked);return false;}
  if(current.generation!=admitted.generation || current.configurationGeneration!=admitted.configurationGeneration) {
    close(AdminNotice::changed);return false;
  }
  const auto currentRevision=backend.revision();
  if(!currentRevision) {close(AdminNotice::changed);return false;}
  if(currentRevision!=revision) {
    const bool reorderSelected=page==Page::entries && cursor && cursor+(typeCursor?2:1)==frame.total;
    const bool advancedSelected=page==Page::entries && typeCursor && cursor+1==frame.total;
    revision=currentRevision;
    if(reorderSelected || advancedSelected) {
      CataloguePage current;
      if(!cataloguePage(catalogue,catalogues[typeCursor],0,allRooms,current)) {close(AdminNotice::changed);return false;}
      cursor=current.total+(advancedSelected?2:1);
    } else if(page==Page::entries && anchor) {
      CataloguePage anchored;
      if(!cataloguePage(catalogue,catalogues[typeCursor],0,allRooms,anchored,anchor,true)) {close(AdminNotice::changed);return false;}
      const auto row=anchored.cursor-anchored.offset;
      cursor=row<anchored.count && anchored.rows[row].id==anchor?anchored.cursor+1:0;
    }
    frame.notice=AdminNotice::changed;render();return false;
  }
  return true;
}
bool Administration::renew() {
  const auto current=backend.authorization();
  if(!current.allowed) {close(AdminNotice::locked);return false;}
  if(current.generation!=admitted.generation || current.configurationGeneration!=admitted.configurationGeneration) {
    close(AdminNotice::changed);return false;
  }
  revision=backend.revision();if(!revision){close(AdminNotice::changed);return false;}return true;
}
void Administration::resume() {
  if(!renew())return;
  page=Page::entries;cursor=0;
  if(returnId) {
    CataloguePage data;
    if(!cataloguePage(catalogue,catalogues[typeCursor],0,allRooms,data,returnId,true)) {close(AdminNotice::changed);return;}
    const auto row=data.cursor-data.offset;
    if(row<data.count && data.rows[row].id==returnId)cursor=data.cursor+1;
  }
  frame.notice=AdminNotice::none;render();
}
void Administration::finishOrder() {
  if(ordering.closed() && ordering.notice()==AdminNotice::none)resume();else render();
}
void Administration::resumeAdvanced() {
  if(typeCursor==moreType) {if(!renew())return;page=Page::more;cursor=0;frame.notice=AdminNotice::none;render();return;}
  resume();
  if(active && !returnId) {CataloguePage data;if(!cataloguePage(catalogue,catalogues[typeCursor],0,allRooms,data)){close(AdminNotice::changed);return;}cursor=data.total+2;render();}
}
void Administration::finishAdvanced() {
  if(advanced.closed() && advanced.notice()==AdminNotice::none)resumeAdvanced();else render();
}
void Administration::tick(uint64_t now) {
  if(!active)return;
  if(page==Page::advanced) {
    if(now<observedAt){close(AdminNotice::expired);return;}observedAt=now;advanced.tick(now);finishAdvanced();
  } else if(page==Page::order) {
    if(now<observedAt) {close(AdminNotice::expired);return;}
    observedAt=now;ordering.tick(now);finishOrder();
  } else if(page==Page::entity) {
    if(now<observedAt) {close(AdminNotice::expired);return;}
    observedAt=now;
    if(form.view().id)returnId=form.view().id;
    form.tick(now);
    if(form.closed() && form.view().notice==AdminNotice::none)resume();else render();
  } else if(guard(now))render();
}
void Administration::turn(int direction,uint64_t now) {
  if(!direction || !guard(now))return;
  backend.activity();
  if(page==Page::advanced) {advanced.turn(direction,now);finishAdvanced();return;}
  if(page==Page::entity) {form.turn(direction,now);render();return;}
  if(page==Page::order) {ordering.turn(direction,now);finishOrder();return;}
  if(page==Page::types) {
    if(direction>0 && typeCursor+1<typeCount)++typeCursor;
    if(direction<0 && typeCursor)--typeCursor;
    frame.notice=AdminNotice::none;render();
  } else if(page==Page::entries) {
    if(direction>0 && cursor+1<frame.total)++cursor;
    if(direction<0 && cursor)--cursor;
    frame.notice=AdminNotice::none;render();
  }
}
void Administration::activate(uint64_t now) {
  if(!guard(now))return;
  backend.activity();
  if(page==Page::advanced) {
    if(advanced.closed()){resumeAdvanced();return;}
    if(frame.id)returnId=frame.id;advanced.activate(now);finishAdvanced();return;
  }
  if(page==Page::more) {returnId=0;advanced.open(Advanced::Scope::repeaters,now);page=Page::advanced;render();return;}
  if(page==Page::order) {
    if(ordering.closed()) {close(AdminNotice::none);return;}
    if(frame.id)returnId=frame.id;
    ordering.activate(now);finishOrder();return;
  }
  if(page==Page::entity) {
    if(form.closed()) {close(AdminNotice::none);return;}
    if(form.view().id)returnId=form.view().id;
    form.activate(now);
    if(form.view().id)returnId=form.view().id;
    if(form.closed() && form.view().notice==AdminNotice::none)resume();else render();
    return;
  }
  if(page==Page::types) {page=typeCursor==moreType?Page::more:Page::entries;cursor=anchor=returnId=0;frame.notice=AdminNotice::none;render();return;}
  if(typeCursor && cursor+1==frame.total) {form.discard();ordering.discard();returnId=0;advanced.open(typeCursor==1?Advanced::Scope::devices:Advanced::Scope::groups,now);page=Page::advanced;render();return;}
  if(cursor+(typeCursor?2:1)==frame.total) {
    form.discard();returnId=0;ordering.open(kinds[typeCursor],now);page=Page::order;render();return;
  }
  if(!cursor) {
    CataloguePage data;
    if(!cataloguePage(catalogue,catalogues[typeCursor],0,allRooms,data)) {close(AdminNotice::changed);return;}
    if(data.total>=capacities[typeCursor]) {frame.notice=AdminNotice::capacity;render();return;}
  }
  const auto selected=frame.id;
  form.open(kinds[typeCursor],selected,now);page=Page::entity;returnId=selected;render();
}
void Administration::back(uint64_t now) {
  if(!guard(now))return;
  backend.activity();
  if(page==Page::advanced) {if(advanced.closed()){resumeAdvanced();return;}if(frame.id)returnId=frame.id;advanced.back(now);finishAdvanced();return;}
  if(page==Page::order) {
    if(ordering.closed()) {close(AdminNotice::none);return;}
    if(frame.id)returnId=frame.id;
    ordering.back(now);finishOrder();return;
  }
  if(page==Page::entity) {
    if(form.closed()) {close(AdminNotice::none);return;}
    if(form.view().id)returnId=form.view().id;
    form.back(now);
    if(form.closed() && form.view().notice==AdminNotice::none)resume();else render();
  } else if(page==Page::entries || page==Page::more) {page=Page::types;cursor=anchor=0;frame.notice=AdminNotice::none;render();}
  else close(AdminNotice::none);
}
}

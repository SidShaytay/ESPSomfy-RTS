#include "LocalAdministration.h"
#include <stdio.h>
#include <string.h>
namespace LocalUi {
using EntityAdmin::Kind;
using EntityAdmin::Mode;
using AdminNotice=EntityAdmin::Notice;
namespace {
constexpr Kind kinds[] = {Kind::room,Kind::shade,Kind::group};
constexpr CatalogueKind catalogues[] = {CatalogueKind::rooms,CatalogueKind::devices,CatalogueKind::groups};
constexpr const char *titles[] = {"Rooms","Devices","Groups"};
constexpr const char *addLabels[] = {"Add room","Add device","Add group"};
constexpr uint8_t capacities[] = {SOMFY_MAX_ROOMS,SOMFY_MAX_SHADES,SOMFY_MAX_GROUPS};
constexpr uint8_t typeCount = sizeof(kinds)/sizeof(*kinds);
}
void Administration::close(AdminNotice notice) {
  form.discard();ordering.discard();frame={};frame.notice=notice;admitted={};
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
  const auto notice=frame.notice;frame={};frame.notice=notice;
  frame.mode=Mode::fields;frame.kind=kinds[typeCursor];
  if(page==Page::types) {
    strcpy(frame.title,"Administration");strcpy(frame.hint,"Press: manage   Back: Home");
    frame.total=frame.rowCount=typeCount;frame.selected=typeCursor;
    for(uint8_t i=0;i<typeCount;i++) {strcpy(frame.rows[i].label,titles[i]);frame.rows[i].selected=i==typeCursor;}
    return;
  }
  CataloguePage data;
  if(!cataloguePage(catalogue,catalogues[typeCursor],0,allRooms,data)) {close(AdminNotice::changed);return;}
  frame.total=data.total+2;if(cursor>=frame.total)cursor=0;
  frame.selected=cursor;frame.offset=cursor/3*3;
  strcpy(frame.title,titles[typeCursor]);strcpy(frame.hint,"Press: choose   Back: types");
  anchor=0;
  for(uint8_t i=frame.offset;i<frame.total && frame.rowCount<3;i++) {
    auto &row=frame.rows[frame.rowCount++];row.selected=i==cursor;
    if(!i) snprintf(row.label,sizeof(row.label),"%s%s",addLabels[typeCursor],data.total>=capacities[typeCursor]?" (full)":"");
    else if(i==data.total+1) strcpy(row.label,"Reorder");
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
  if(page==Page::entity || page==Page::order)return true; // Each child owns its captured history/result.
  const auto current=backend.authorization();
  if(!current.allowed) {close(AdminNotice::locked);return false;}
  if(current.generation!=admitted.generation || current.configurationGeneration!=admitted.configurationGeneration) {
    close(AdminNotice::changed);return false;
  }
  const auto currentRevision=backend.revision();
  if(!currentRevision) {close(AdminNotice::changed);return false;}
  if(currentRevision!=revision) {
    const bool reorderSelected=page==Page::entries && cursor && cursor+1==frame.total;
    revision=currentRevision;
    if(reorderSelected) {
      CataloguePage current;
      if(!cataloguePage(catalogue,catalogues[typeCursor],0,allRooms,current)) {close(AdminNotice::changed);return false;}
      cursor=current.total+1;
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
void Administration::resume() {
  const auto current=backend.authorization();
  if(!current.allowed) {close(AdminNotice::locked);return;}
  if(current.generation!=admitted.generation || current.configurationGeneration!=admitted.configurationGeneration) {
    close(AdminNotice::changed);return;
  }
  page=Page::entries;revision=backend.revision();cursor=0;
  if(!revision) {close(AdminNotice::changed);return;}
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
void Administration::tick(uint64_t now) {
  if(!active)return;
  if(page==Page::order) {
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
  if(page==Page::types) {page=Page::entries;cursor=anchor=returnId=0;frame.notice=AdminNotice::none;render();return;}
  if(cursor+1==frame.total) {
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
  } else if(page==Page::entries) {page=Page::types;cursor=anchor=0;frame.notice=AdminNotice::none;render();}
  else close(AdminNotice::none);
}
}

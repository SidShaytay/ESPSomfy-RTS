#include "LocalCalibration.h"
#include "LiftGuideView.h"
#include <new>
#include <stdio.h>
#include <string.h>
namespace LocalUi { namespace Calibration {
using EntityAdmin::Notice;
struct Form::Session {
  enum class Page : uint8_t { picker, kind, prepare, guide } page = Page::picker;
  EntityAdmin::Authorization auth;
  uint64_t revision = 0, observedAt = 0, activityAt = 0;
  uint32_t token = 0;
  uint8_t cursor = 0, id = 0;
  bool targeted = false;
  LiftKind kind=LiftKind::direct;
  LiftGuideSnapshot state;
};
void Form::close(Notice notice, bool cancel) {
  if(session) {
    if(cancel && session->token && backend)
      backend->act(session->token,LiftGuideAction::cancel,session->auth,false);
    session->~Session(); allocator.deallocate(session); session = nullptr;
  }
  status = notice;
}
bool Form::open(uint64_t now,uint8_t id) {
  discard();
  if(!backend) { status = Notice::invalid; return false; }
  const auto auth = backend->authorization();
  if(!auth.allowed || !auth.generation || !auth.configurationGeneration) { status = Notice::locked; return false; }
  const auto revision = backend->revision();
  if(!revision) { status = Notice::changed; return false; }
  void *storage = allocator.allocate(sizeof(Session));
  if(!storage) { status = Notice::noMemory; return false; }
  session = new(storage) Session{};
  if(id && !backend->available(id)) {close(Notice::invalid);return false;}
  if(id) {session->targeted=true;session->id=id;session->page=Session::Page::kind;}
  session->auth = auth; session->revision = revision;
  session->observedAt = session->activityAt = now;
  return true;
}
bool Form::guard(uint64_t now) {
  if(!session || !backend) return false;
  auto &s = *session;
  const auto auth = backend->authorization();
  if(!auth.allowed) { close(Notice::locked,false); return false; }
  if(auth.generation != s.auth.generation || auth.configurationGeneration != s.auth.configurationGeneration) {
    close(Notice::changed,false); return false;
  }
  if(now < s.observedAt || now - s.activityAt >= LiftGuideSession::maximumMs) {
    close(Notice::expired); return false;
  }
  s.observedAt = now;
  if(!s.token) {
    if(backend->revision() != s.revision) { close(Notice::changed,false); return false; }
    return true;
  }
  LiftGuideSnapshot current;
  if(!backend->snapshot(s.token,s.auth,current) || current.id != s.id || current.token != s.token) {
    close(Notice::changed,false); return false;
  }
  if(current.stage != s.state.stage) s.cursor = 0;
  s.state = current;
  return true;
}
void Form::tick(uint64_t now) { guard(now); }
void Form::turn(int direction, uint64_t now) {
  if(!direction || !guard(now)) return;
  auto &s = *session;
  uint8_t count = 0;
  if(s.page == Session::Page::picker) {
    CataloguePage page;
    if(!cataloguePage(catalogue,CatalogueKind::devices,0,allRooms,page)) { close(Notice::changed); return; }
    count = page.total;
  } else if(s.page == Session::Page::kind) count = 4;
  else if(s.page == Session::Page::prepare) count=2;
  else {
    count = liftGuideView(s.state.stage,s.state.applied,s.state.durable,s.state.reversalsKnown).count;
    if(s.state.stage == LiftGuideStage::review) count += 5;
  }
  if(direction > 0 && s.cursor + 1 < count) ++s.cursor;
  if(direction < 0 && s.cursor) --s.cursor;
  s.activityAt = now; backend->activity(); status = Notice::none;
}
void Form::activate(uint64_t now, bool human) {
  if(!guard(now)) return;
  auto &s = *session;
  s.activityAt = now; backend->activity(); status = Notice::none;
  if(s.page == Session::Page::picker) {
    CataloguePage page;
    if(!cataloguePage(catalogue,CatalogueKind::devices,s.cursor,allRooms,page) || s.cursor >= page.total) return;
    const auto &item = page.rows[s.cursor - page.offset];
    if(!backend->available(item.id)) { status = Notice::invalid; return; }
    s.id = item.id; s.cursor = 0; s.page = Session::Page::kind;
    return;
  }
  if(s.page == Session::Page::kind) {
    if(s.cursor == 3) { close(Notice::none); return; }
    s.kind=s.cursor?LiftKind::segmented:LiftKind::direct;
    s.page=Session::Page::prepare;s.cursor=0;return;
  }
  if(s.page==Session::Page::prepare) {
    if(s.cursor) {close(Notice::none);return;}
    uint32_t token = 0;
    if(!backend->begin(s.id,s.kind,s.auth,token) || !token) {
      status = Notice::busy; return;
    }
    s.token = token; s.page = Session::Page::guide; s.cursor = 0;
    guard(now); return;
  }
  const auto view = liftGuideView(s.state.stage,s.state.applied,s.state.durable,s.state.reversalsKnown);
  if(s.cursor >= view.count) return;
  const auto action = view.choices[s.cursor].action;
  if(!backend->act(s.token,action,s.auth,human)) { status = Notice::invalid; guard(now); return; }
  if(action == LiftGuideAction::cancel) { s.token = 0; close(Notice::none); return; }
  guard(now);
}
void Form::back(uint64_t now) {
  if(!guard(now)) return;
  backend->activity();
  if(session->page == Session::Page::kind && !session->targeted) { session->page = Session::Page::picker; session->cursor = 0; return; }
  close(Notice::none);
}
void Form::render(EntityAdmin::FormView &out) {
  out = {}; out.notice = status; out.mode = EntityAdmin::Mode::stackedRows; out.protectedFrame = true;
  if(!session) return;
  auto &s = *session;
  out.kind = EntityAdmin::Kind::shade; out.id = s.id; out.selected = s.cursor; out.offset = s.cursor / 3 * 3;
  if(s.page == Session::Page::picker) {
    snprintf(out.title,sizeof(out.title),"Calibrate shutter");
    snprintf(out.hint,sizeof(out.hint),"Choose one shutter. Groups use its saved calibration.");
    CataloguePage page;
    if(!cataloguePage(catalogue,CatalogueKind::devices,s.cursor,allRooms,page)) { close(Notice::changed); return; }
    out.total = page.total;
    if(!out.total) { out.rowCount = 1; snprintf(out.rows[0].label,sizeof(out.rows[0].label),"No devices configured"); return; }
    for(unsigned i = 0; i < page.count && i < 3; ++i) {
      const auto &item = page.rows[i]; auto &row = out.rows[out.rowCount++];
      snprintf(row.label,sizeof(row.label),"#%u %s",unsigned(item.id),item.name);
      row.selected = page.offset + i == s.cursor;
      if(!backend->available(item.id)) snprintf(row.value,sizeof(row.value),"Unavailable for this motor");
      if(row.selected) out.id = item.id;
    }
    return;
  }
  if(s.page==Session::Page::prepare) {
    snprintf(out.title,sizeof(out.title),"Before you start");
    snprintf(out.hint,sizeof(out.hint),"Stop stops. Back discards, but does not stop movement.");
    out.total=2;out.rowCount=3;
    snprintf(out.rows[0].label,sizeof(out.rows[0].label),"Start guided checks");
    snprintf(out.rows[0].value,sizeof(out.rows[0].value),"Watch bottom edge; curtain: side");
    snprintf(out.rows[1].label,sizeof(out.rows[1].label),"Cancel");
    snprintf(out.rows[1].value,sizeof(out.rows[1].value),"Keep current calibration");
    snprintf(out.rows[2].label,sizeof(out.rows[2].label),"Halfway = half the opening");
    snprintf(out.rows[2].value,sizeof(out.rows[2].value),"By distance, not by time");
    out.rows[s.cursor].selected=true;return;
  }
  if(s.page == Session::Page::kind) {
    snprintf(out.title,sizeof(out.title),"What kind of cover?");
    snprintf(out.hint,sizeof(out.hint),"Choose what you see. Next: guided movement checks.");
    const char *labels[] = {"Fabric roller blind","Shutter with linked slats","Curtain / awning","Cancel"};
    const char *descriptions[]={"One continuous sheet of fabric","Separate strips with gaps","Fabric opens sideways / retracts","Keep current calibration"};
    out.total = 4;out.offset=s.cursor/3*3;
    for(unsigned i = out.offset; i < 4 && out.rowCount<3; ++i) { snprintf(out.rows[out.rowCount].label,sizeof(out.rows[out.rowCount].label),"%s",labels[i]); out.rows[out.rowCount].selected = i == s.cursor;
      snprintf(out.rows[out.rowCount].value,sizeof(out.rows[out.rowCount].value),"%s",descriptions[i]);++out.rowCount; }
    return;
  }
  const auto view = liftGuideView(s.state.stage,s.state.applied,s.state.durable,s.state.reversalsKnown);
  snprintf(out.title,sizeof(out.title),"%s",view.title); snprintf(out.hint,sizeof(out.hint),"%s",view.hint);
  const uint8_t details = s.state.stage == LiftGuideStage::review ? 5 : 0;
  out.total = details + view.count;
  const auto &c = s.state.calibration;
  const char *labels[] = {"Up / down total","Take-up up / down","Halfway up / down","Settling up / down","Reversal up / down"};
  const uint32_t up[] = {s.state.upTime,c.up.takeupMs,c.up.midpointMs,c.up.settlingMs,c.up.reversalMs};
  const uint32_t down[] = {s.state.downTime,c.down.takeupMs,c.down.midpointMs,c.down.settlingMs,c.down.reversalMs};
  for(unsigned i = out.offset; i < out.total && out.rowCount < 3; ++i) {
    auto &row = out.rows[out.rowCount++]; row.selected = i == s.cursor;
    if(i >= view.count) {
      const unsigned detail=i-view.count;
      snprintf(row.label,sizeof(row.label),"%s",labels[detail]);
      if(detail == 4 && !s.state.reversalsKnown) snprintf(row.value,sizeof(row.value),"Not measured");
      else snprintf(row.value,sizeof(row.value),"%lu / %lu ms",static_cast<unsigned long>(up[detail]),static_cast<unsigned long>(down[detail]));
    } else snprintf(row.label,sizeof(row.label),"%s",view.choices[i].label);
  }
}
} }

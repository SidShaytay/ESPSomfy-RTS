#include <unity.h>
#include "LocalUi.h"
#include "LocalUiMemory.h"
#include <string.h>
using namespace LocalUi;
void setUp() {} void tearDown() {}
struct Fixture : CatalogueSource {
  CatalogueEntry devices[32]={},groups[16]={},rooms[16]={};
  Model base;
  Fixture() {
    devices[3]={7,0,0,"Bedroom",700,1,35,true,true};
    base.ready=base.controlsEnabled=base.authorized=true;
    base.authMode=AuthMode::none; base.authGeneration=2; base.authConfigurationGeneration=1;
    base.modelRevision=4; base.restoreRevision=1; base.anyMoving=true;
    base.target.ref={EntityKind::device,7}; strcpy(base.target.name,"Bedroom");
    base.target.identity=700; base.target.revision=1; base.target.token=99;
    base.target.exists=base.target.valid=base.target.moving=true;
    base.target.capabilities=7; base.target.position=35;
  }
  bool read(CatalogueKind kind,uint8_t slot,CatalogueEntry &out) override {
    out=kind==CatalogueKind::devices?devices[slot]:kind==CatalogueKind::groups?groups[slot]:rooms[slot];
    return out.id!=0;
  }
  Model snapshot(const View &v) {
    Model m=base; m.catalogue=v.catalogue; m.room=v.room;
    m.pageValid=v.catalogue==CatalogueKind::rooms?roomCataloguePage(*this,v.cursor,m.page,v.anchorId,v.anchored):
      cataloguePage(*this,v.catalogue,v.cursor,v.room,m.page,v.anchorId,v.anchored,v.anchorGroup);
    if(base.quickReady && (v.screen==Screen::quickHome || v.screen==Screen::quickManage || v.screen==Screen::quickActions || v.screen==Screen::quickSave)) {
      m.page={};m.page.count=3;m.page.total=5;m.pageValid=true;
      const auto &bindings=v.quickEditing?v.quickDraft:m.quickAccess;
      for(size_t i=0;i<quickAccessSlots;i++) {
        const auto &b=*bindings.at(i);const auto &source=b.ref.kind==EntityKind::group?groups:devices;
        const size_t cap=b.ref.kind==EntityKind::group?16:32;
        for(size_t j=0;j<cap;j++)if(source[j].id==b.ref.id && source[j].identity==b.identity)m.page.rows[i]=source[j];
      }
    }
    m.pageToken=11;
    if(v.entity!=m.target.ref) m.target={};
    return m;
  }
};
struct Drive {
  Controller c; Fixture f; uint32_t now=0;
  void tick(int8_t turn=0,bool pressed=false,bool visible=true,bool back=false,uint32_t advance=50) {
    now+=advance; auto m=f.snapshot(c.view()); c.tick(now,m,turn,pressed,visible,back);
  }
  void holdSend() {tick(0,true);tick(0,true);tick(0,true,true,false,800);}
  void click(bool visible=true) { tick(0,true,visible);tick(0,true,visible);tick(0,false,visible);tick(0,false,visible); }
  void back() { tick(0,false,true,true);tick(0,false,true,true);tick();tick(); }
  void manageQuick(){while(c.view().selected>3)tick(-1);while(c.view().selected<3)tick(1);click();TEST_ASSERT_EQUAL(Screen::quickManage,c.view().screen);}
  void home() { tick(); tick(1); }
  void devices(bool visible=true) { home(); tick(1); click(); click(visible); tick(); }
};
static void administration_requires_unlock_and_forwards_only_presented_form_press() {
  Drive d;d.home();for(unsigned i=0;i<administrationHomeIndex;i++)d.tick(1);
  d.click(false);TEST_ASSERT_EQUAL(Screen::home,d.c.view().screen);
  d.f.base.authorized=false;d.f.base.authMode=AuthMode::pin;d.click();
  TEST_ASSERT_EQUAL(Screen::unlock,d.c.view().screen);
  d.c.unlockResult(d.now,AuthResult::unlocked);d.f.base.authorized=true;d.tick();d.click();
  TEST_ASSERT_EQUAL(Screen::administration,d.c.view().screen);
  FormInput input;TEST_ASSERT_FALSE(d.c.takeFormInput(input));
  d.tick(1);TEST_ASSERT_TRUE(d.c.takeFormInput(input));TEST_ASSERT_EQUAL(1,input.turn);
  d.click(false);TEST_ASSERT_FALSE(d.c.takeFormInput(input));
  d.click();TEST_ASSERT_TRUE(d.c.takeFormInput(input));TEST_ASSERT_TRUE(input.activate);
  d.back();TEST_ASSERT_TRUE(d.c.takeFormInput(input));TEST_ASSERT_TRUE(input.back);
  d.c.closeForm();TEST_ASSERT_EQUAL(Screen::home,d.c.view().screen);
  TEST_ASSERT_EQUAL(administrationHomeIndex,d.c.view().selected);
  Command command;TEST_ASSERT_FALSE(d.c.take(command));
}
static void stable_catalogue_selection_binds_id_not_slot() {
  Drive d; d.devices();
  TEST_ASSERT_EQUAL(int(Screen::control),int(d.c.view().screen));
  TEST_ASSERT_EQUAL(7,d.c.view().entity.id); TEST_ASSERT_EQUAL(99,d.c.view().openedToken);
  Command out; TEST_ASSERT_FALSE(d.c.take(out));
  d.click(); TEST_ASSERT_TRUE(d.c.take(out));
  TEST_ASSERT_EQUAL(7,out.ref.id); TEST_ASSERT_EQUAL(99,out.token);
  TEST_ASSERT_EQUAL(int(Action::stop),int(out.action));
}
static void never_presented_catalogue_or_stop_emits_no_intent() {
  Drive d; d.devices(false);
  TEST_ASSERT_EQUAL(int(Screen::catalogue),int(d.c.view().screen));
  d.click(); d.tick();
  TEST_ASSERT_EQUAL(int(Screen::control),int(d.c.view().screen));
  d.click(false);
  Command out; TEST_ASSERT_FALSE(d.c.take(out));
  TEST_ASSERT_EQUAL(int(Notice::wait),int(d.c.view().notice));
  d.click(true); TEST_ASSERT_TRUE(d.c.take(out));
}
static void empty_full_and_live_reordered_catalogues_are_bounded() {
  Drive d; d.f.devices[3]={}; d.home(); d.tick(1); d.click(); d.tick();
  TEST_ASSERT_EQUAL(0,d.c.view().cursor); d.click(); Command out; TEST_ASSERT_FALSE(d.c.take(out));
  for(uint8_t i=0;i<32;i++) d.f.devices[i]={uint8_t(i+1),0,int8_t(i),"Same name",uint32_t(100+i),1};
  d.tick();
  for(unsigned i=0;i<40;i++) d.tick(1);
  TEST_ASSERT_EQUAL(31,d.c.view().cursor); d.tick(1); TEST_ASSERT_EQUAL(31,d.c.view().cursor);
  d.f.devices[31].sortOrder=-1; d.tick(); TEST_ASSERT_EQUAL(0,d.c.view().cursor);
  TEST_ASSERT_EQUAL(32,d.c.view().anchorId);
  d.f.devices[31]={}; d.tick(); TEST_ASSERT_EQUAL(0,d.c.view().cursor);
}
static void target_replacement_between_row_and_focus_is_not_rebound() {
  Drive d; d.home(); d.tick(1); d.click(); d.click();
  d.f.base.target.identity=701; ++d.f.base.target.revision; ++d.f.base.target.token;
  d.tick(); d.click(); Command out; TEST_ASSERT_FALSE(d.c.take(out));
  TEST_ASSERT_EQUAL(0,d.c.view().openedToken);
}
static void open_context_change_does_not_reseed_or_retarget() {
  Drive d; d.devices(); ++d.f.base.target.token; ++d.f.base.target.revision;
  d.tick(); d.click(); Command out; TEST_ASSERT_FALSE(d.c.take(out));
  TEST_ASSERT_EQUAL(99,d.c.view().openedToken);
  d.f.base.expiredSessionGeneration=d.f.base.authGeneration; d.f.base.authorized=false; d.f.base.target.stopLease=true; d.click();
  TEST_ASSERT_FALSE(d.c.take(out)); TEST_ASSERT_EQUAL(int(Notice::changed),int(d.c.view().notice));
}
static void locked_selection_unlocks_without_replaying() {
  Drive d; d.f.base.authorized=false; d.f.base.authMode=AuthMode::pin; d.devices(); d.click();
  TEST_ASSERT_EQUAL(int(Screen::unlock),int(d.c.view().screen));
  d.tick(1); for(unsigned i=0;i<4;i++) d.click();
  for(unsigned i=0;i<RotaryKeyboard::actionOffset(d.c.view().credentialKeyboard)+5-1;i++) d.tick(1);
  d.click(); Credentials given; TEST_ASSERT_TRUE(d.c.takeCredentials(given));
  TEST_ASSERT_EQUAL_STRING("1111",given.secret);
  d.c.unlockResult(d.now,AuthResult::unlocked);
  d.f.base.authorized=true; ++d.f.base.authGeneration; d.tick();
  TEST_ASSERT_TRUE(d.c.view().authorizedSelection);
  Command out; TEST_ASSERT_FALSE(d.c.take(out));
  d.click(); TEST_ASSERT_TRUE(d.c.take(out));
}
static void locked_stop_lease_never_authorizes_another_action() {
  Drive d; d.devices(); d.f.base.expiredSessionGeneration=d.f.base.authGeneration; d.f.base.authorized=false; d.f.base.target.stopLease=true;
  d.click(); Command out; TEST_ASSERT_TRUE(d.c.take(out));
  d.tick(-1); d.click(); TEST_ASSERT_FALSE(d.c.take(out));
  TEST_ASSERT_EQUAL(int(Screen::unlock),int(d.c.view().screen));
  Drive missing;missing.devices();missing.f.base.authorized=false;missing.f.base.target.stopLease=true;
  missing.click();TEST_ASSERT_FALSE(missing.c.take(out)); // old aggregate source0 fails closed
  Drive source;source.devices();source.f.base.expiredSessionGeneration=source.f.base.authGeneration;
  source.f.base.authorized=false;source.f.base.target.stopLease=true;++source.f.base.authGeneration;
  source.click();TEST_ASSERT_TRUE(source.c.take(out));TEST_ASSERT_EQUAL(source.f.base.expiredSessionGeneration,out.authGeneration);
}
static void secrets_wipe_on_cancel_configuration_change_sleep_and_timeout() {
  for(unsigned reason=0;reason<4;reason++) {
    Drive d; d.f.base.authorized=false; d.f.base.authMode=AuthMode::pin; d.devices();d.click();
    d.tick(1); d.click(); TEST_ASSERT_EQUAL(1,d.c.view().credentialLength);
    if(reason==0) d.back();
    else if(reason==1) { ++d.f.base.authConfigurationGeneration; d.tick(); }
    else { d.f.base.anyMoving=reason==3; d.tick(0,false,true,false,reason==3?31000:21000); }
    TEST_ASSERT_NOT_EQUAL(int(Screen::unlock),int(d.c.view().screen));
    TEST_ASSERT_EQUAL(0,d.c.view().credentialLength); Credentials given; TEST_ASSERT_FALSE(d.c.takeCredentials(given));
    Command out; TEST_ASSERT_FALSE(d.c.take(out));
  }
}
static void wake_holds_and_mixed_gestures_do_not_emit() {
  Drive d; d.f.base.anyMoving=false; d.tick(); d.tick(0,false,true,false,21000);
  TEST_ASSERT_EQUAL(0,d.c.brightness()); d.click();
  TEST_ASSERT_EQUAL(int(Screen::status),int(d.c.view().screen));
  Command out; TEST_ASSERT_FALSE(d.c.take(out));
  Drive held; held.tick(0,true);held.tick(0,true,true,false,900);held.tick();held.tick();
  TEST_ASSERT_EQUAL(int(Screen::status),int(held.c.view().screen));
  held.devices(); held.tick(-1); held.tick(0,true);held.tick(1,true);held.tick();held.tick();
  TEST_ASSERT_FALSE(held.c.take(out));
}
static void display_wheel_selects_rows_and_only_explicit_save_persists() {
  Drive d;d.home();for(unsigned i=0;i<4;i++)d.tick(1);d.click();
  TEST_ASSERT_EQUAL(Screen::brightness,d.c.view().screen);
  d.tick(1);
  TEST_ASSERT_EQUAL(1,d.c.view().setting);
  TEST_ASSERT_EQUAL(32,d.c.preference());
  TEST_ASSERT_EQUAL(10,d.c.preferences().dimSeconds);
  d.click();d.tick(1); // Edit Dim after.
  TEST_ASSERT_EQUAL(1,d.c.view().setting);
  TEST_ASSERT_EQUAL(15,d.c.preferences().dimSeconds);
  d.click();d.tick(1); // Return to selection, then select Sleep after.
  TEST_ASSERT_EQUAL(2,d.c.view().setting);
  TEST_ASSERT_EQUAL(20,d.c.preferences().offSeconds);
  d.click();d.tick(-1);d.click();
  TEST_ASSERT_EQUAL(15,d.c.preferences().offSeconds);
  DisplayPreferences saved;TEST_ASSERT_FALSE(d.c.takePreferences(saved));
  d.tick(1); // Save row.
  d.click(false);TEST_ASSERT_EQUAL(Screen::brightness,d.c.view().screen);
  TEST_ASSERT_FALSE(d.c.takePreferences(saved));
  d.click();TEST_ASSERT_TRUE(d.c.takePreferences(saved));
  TEST_ASSERT_EQUAL(15,saved.dimSeconds);TEST_ASSERT_EQUAL(15,saved.offSeconds);
  TEST_ASSERT_FALSE(d.c.takePreferences(saved));
}
static void display_cancel_timeout_and_reentry_discard_edits() {
  for(unsigned reason=0;reason<3;++reason) {
    Drive d;d.home();for(unsigned i=0;i<4;i++)d.tick(1);d.click();
    d.click(false);d.tick(1); // An unseen press must not enter editing.
    TEST_ASSERT_EQUAL(32,d.c.preference());TEST_ASSERT_EQUAL(1,d.c.view().setting);
    d.tick(-1);d.click();d.tick(1);TEST_ASSERT_EQUAL(38,d.c.preference());
    if(reason==0) {d.click();for(unsigned i=0;i<4;i++)d.tick(1);d.tick(1);TEST_ASSERT_EQUAL(4,d.c.view().setting);d.click();}
    else if(reason==1)d.back();
    else {d.f.base.anyMoving=false;d.tick(0,false,true,false,21000);}
    DisplayPreferences saved;TEST_ASSERT_FALSE(d.c.takePreferences(saved));TEST_ASSERT_EQUAL(32,d.c.preference());
    if(reason<2) {d.click();d.tick(1);TEST_ASSERT_EQUAL(1,d.c.view().setting);TEST_ASSERT_EQUAL(32,d.c.preference());}
  }
}
static void brightness_back_cancels_and_explicit_save_reports_failure() {
  Drive d; d.home(); for(unsigned i=0;i<4;i++) d.tick(1); d.click();
  TEST_ASSERT_EQUAL(int(Screen::brightness),int(d.c.view().screen));
  d.click();d.tick(1); TEST_ASSERT_EQUAL(38,d.c.preference()); d.back();
  DisplayPreferences saved; TEST_ASSERT_FALSE(d.c.takePreferences(saved)); TEST_ASSERT_EQUAL(32,d.c.preference());
  d.click();d.click();d.tick(1);d.click();for(unsigned i=0;i<3;i++)d.tick(1);d.click();
  TEST_ASSERT_TRUE(d.c.takePreferences(saved)); TEST_ASSERT_EQUAL(38,saved.brightness);
  d.c.brightnessResult(d.now,false); TEST_ASSERT_EQUAL(int(Notice::saveFailed),int(d.c.view().notice));
}
static void privileged_brightness_draft_reverts_on_expiry_or_configuration_change() {
  for(unsigned reason=0;reason<2;reason++) {
    Drive d; d.home();for(unsigned i=0;i<4;i++) d.tick(1);d.click();d.click();d.tick(1);
    if(reason==0) d.f.base.authorized=false; else ++d.f.base.authConfigurationGeneration;
    d.tick(1); TEST_ASSERT_NOT_EQUAL(int(Screen::brightness),int(d.c.view().screen));
    TEST_ASSERT_EQUAL(32,d.c.preference()); DisplayPreferences saved;TEST_ASSERT_FALSE(d.c.takePreferences(saved));
  }
}
static void settings_receives_logical_events_and_closes_explicitly() {
  Drive d;d.home(); for(unsigned i=0;i<3;i++) d.tick(1); d.click();
  TEST_ASSERT_EQUAL(int(Screen::settings),int(d.c.view().screen));
  FormInput event; TEST_ASSERT_FALSE(d.c.takeFormInput(event));
  d.tick(1);TEST_ASSERT_TRUE(d.c.takeFormInput(event));TEST_ASSERT_EQUAL(1,event.turn);
  d.click();TEST_ASSERT_TRUE(d.c.takeFormInput(event));TEST_ASSERT_TRUE(event.activate);
  d.back();TEST_ASSERT_TRUE(d.c.takeFormInput(event));TEST_ASSERT_TRUE(event.back);
  TEST_ASSERT_EQUAL(int(Screen::settings),int(d.c.view().screen));
  d.c.closeForm(); TEST_ASSERT_EQUAL(int(Screen::home),int(d.c.view().screen));
}
static void encoder_detents_bounce_and_memory_failures_remain_bounded() {
  Encoder e;TEST_ASSERT_EQUAL(0,e.sample(3));TEST_ASSERT_EQUAL(0,e.sample(1));TEST_ASSERT_EQUAL(0,e.sample(3));
  TEST_ASSERT_EQUAL(0,e.sample(1));TEST_ASSERT_EQUAL(1,e.sample(0));TEST_ASSERT_EQUAL(0,e.sample(2));
  TEST_ASSERT_EQUAL(1,e.sample(3));TEST_ASSERT_EQUAL(0,e.sample(2));TEST_ASSERT_EQUAL(-1,e.sample(0));
  GraphicsMemory missing; missing.begin(false,[](size_t)->void*{return nullptr;});
  TEST_ASSERT_EQUAL(int(GraphicsFailure::missing),int(missing.failure));
  GraphicsMemory failed;failed.begin(true,[](size_t)->void*{return nullptr;});
  TEST_ASSERT_EQUAL(int(GraphicsFailure::allocation),int(failed.failure));
}
static void openMore(Drive &d) {
  d.f.base.target.capabilities=allControlActions & ~controlActionBit(Action::toggle);
  d.devices(); d.tick(1);d.tick(1);d.click();
  TEST_ASSERT_EQUAL(int(Screen::actions),int(d.c.view().screen));
}
static void openFavourite(Drive &d,Action action=Action::setFavouriteHere) {
  d.f.base.target.favouriteRevision=71;d.f.base.target.favouriteReady=true;d.f.base.target.favouriteClearReady=true;
  d.f.base.target.moving=false;d.f.base.anyMoving=false;
  openMore(d);for(unsigned i=0;i<unsigned(Action::count)&&d.c.view().action!=action;++i)d.tick(1);
  TEST_ASSERT_EQUAL(action,d.c.view().action);d.click();
  TEST_ASSERT_EQUAL(Screen::favouriteObserve,d.c.view().screen);
}
static void favourite_requires_human_observation_then_separate_displayed_confirmation() {
  Drive d;openFavourite(d);Command out;
  TEST_ASSERT_EQUAL(0,d.c.view().valueChoice);TEST_ASSERT_FALSE(d.c.take(out));
  d.tick(1);d.click(false);TEST_ASSERT_EQUAL(Screen::favouriteObserve,d.c.view().screen);
  d.click();TEST_ASSERT_EQUAL(Screen::favouriteConfirm,d.c.view().screen);
  TEST_ASSERT_EQUAL(0,d.c.view().valueChoice);TEST_ASSERT_FALSE(d.c.take(out));
  d.tick(1);d.click(false);TEST_ASSERT_FALSE(d.c.take(out));
  d.click();TEST_ASSERT_TRUE(d.c.take(out));TEST_ASSERT_EQUAL(Action::setFavouriteHere,out.action);
  TEST_ASSERT_EQUAL(71,out.favouriteRevision);TEST_ASSERT_EQUAL(Screen::control,d.c.view().screen);
  TEST_ASSERT_EQUAL(Action::stop,d.c.view().action);
  Drive cancel;openFavourite(cancel,Action::clearFavouriteHere);cancel.tick(1);cancel.click();cancel.click();
  TEST_ASSERT_FALSE(cancel.c.take(out));TEST_ASSERT_EQUAL(Screen::control,cancel.c.view().screen);
}
static void favourite_observation_cannot_survive_motion_expiry_or_context_change() {
  for(unsigned failure=0;failure<5;++failure) {
    Drive d;openFavourite(d);d.tick(1);d.click();TEST_ASSERT_EQUAL(Screen::favouriteConfirm,d.c.view().screen);
    if(failure==0)++d.f.base.target.favouriteRevision;
    if(failure==1)d.f.base.authorized=false;
    if(failure==2)++d.f.base.authConfigurationGeneration;
    if(failure==3)d.f.base.target.favouriteReady=false;
    if(failure==4)++d.f.base.target.token;
    d.tick();TEST_ASSERT_EQUAL(Screen::control,d.c.view().screen);TEST_ASSERT_EQUAL(Action::stop,d.c.view().action);
    Command out;TEST_ASSERT_FALSE(d.c.take(out));
  }
  Drive moving;openFavourite(moving);moving.f.base.target.favouriteReady=false;
  moving.tick(1);moving.click();TEST_ASSERT_EQUAL(Screen::favouriteObserve,moving.c.view().screen);
  Command out;TEST_ASSERT_FALSE(moving.c.take(out));moving.back();TEST_ASSERT_EQUAL(Screen::control,moving.c.view().screen);
}
static void numeric_control_draft_requires_displayed_apply_and_cancel_never_emits() {
  Drive d;openMore(d);d.click();
  TEST_ASSERT_EQUAL(int(Screen::value),int(d.c.view().screen));
  TEST_ASSERT_EQUAL(35,d.c.view().draftValue);
  d.tick(1);Command out;TEST_ASSERT_FALSE(d.c.take(out));
  d.click();TEST_ASSERT_EQUAL(2,d.c.view().valueChoice); // confirmation defaults Cancel
  d.tick(-1);d.click(false);TEST_ASSERT_FALSE(d.c.take(out));
  d.click();TEST_ASSERT_TRUE(d.c.take(out));
  TEST_ASSERT_EQUAL(int(Action::position),int(out.action));TEST_ASSERT_EQUAL(36,out.value);
  TEST_ASSERT_EQUAL(int(Screen::control),int(d.c.view().screen));
  TEST_ASSERT_EQUAL(int(Action::stop),int(d.c.view().action));
  Drive cancel;openMore(cancel);cancel.click();cancel.tick(1);cancel.back();
  TEST_ASSERT_EQUAL(int(Screen::control),int(cancel.c.view().screen));TEST_ASSERT_FALSE(cancel.c.take(out));
  Drive defaultCancel;openMore(defaultCancel);defaultCancel.click();defaultCancel.click();defaultCancel.click();
  TEST_ASSERT_EQUAL(int(Screen::control),int(defaultCancel.c.view().screen));TEST_ASSERT_FALSE(defaultCancel.c.take(out));
}
static void numeric_control_drafts_discard_on_context_authorization_and_timeout() {
  for(unsigned reason=0;reason<4;reason++) {
    Drive d;openMore(d);d.click();d.tick(1);
    if(reason==0) ++d.f.base.target.token;
    if(reason==1) d.f.base.authorized=false;
    if(reason==2) ++d.f.base.authGeneration;
    d.tick(0,false,true,false,reason==3?30000:50);
    TEST_ASSERT_EQUAL(int(Screen::control),int(d.c.view().screen));
    Command out;TEST_ASSERT_FALSE(d.c.take(out));TEST_ASSERT_EQUAL(0,d.c.view().draftValue);
  }
}
static void toggle_and_capability_menus_have_no_invisible_fallback() {
  Drive d;d.f.base.target.capabilities=controlActionBit(Action::toggle);d.devices();
  TEST_ASSERT_EQUAL(int(Action::toggle),int(d.c.view().action));
  d.tick(1);d.click(false);Command out;TEST_ASSERT_FALSE(d.c.take(out));
  d.click();TEST_ASSERT_TRUE(d.c.take(out));TEST_ASSERT_EQUAL(int(Action::toggle),int(out.action));
  d.click();TEST_ASSERT_TRUE(d.c.take(out));TEST_ASSERT_EQUAL(int(Action::toggle),int(out.action));
  Drive extras;openMore(extras);extras.tick(1); // tilt up after position
  extras.click(false);TEST_ASSERT_FALSE(extras.c.take(out));
  extras.click();TEST_ASSERT_TRUE(extras.c.take(out));TEST_ASSERT_EQUAL(int(Action::tiltUp),int(out.action));
}
static void step_bounds_and_tilt_stop_never_borrow_stop_grace() {
  Drive d;openMore(d);
  // Extra order: position, tilt up/STOP/down/position, MY, Favorite, step up/down, sun on/off.
  for(unsigned i=0;i<7;i++) d.tick(1);
  TEST_ASSERT_EQUAL(int(Action::stepUp),int(d.c.view().action));d.click();
  for(unsigned i=0;i<140;i++) d.tick(1);
  TEST_ASSERT_EQUAL(127,d.c.view().draftValue);d.click();d.tick(-1);d.click();
  Command out;TEST_ASSERT_TRUE(d.c.take(out));TEST_ASSERT_EQUAL(127,out.step);TEST_ASSERT_EQUAL(-1,out.value);
  Drive tilt;openMore(tilt);tilt.tick(1);tilt.tick(1);
  tilt.f.base.target.tiltMoving=true;tilt.f.base.expiredSessionGeneration=tilt.f.base.authGeneration;
  tilt.f.base.authorized=false;tilt.f.base.target.stopLease=true;
  tilt.click();TEST_ASSERT_FALSE(tilt.c.take(out));TEST_ASSERT_EQUAL(int(Screen::unlock),int(tilt.c.view().screen));
  Drive unknown;openMore(unknown);unknown.f.base.target.position=-1;unknown.click();
  TEST_ASSERT_EQUAL(int(Screen::actions),int(unknown.c.view().screen));
  TEST_ASSERT_EQUAL(int(Notice::valueUnavailable),int(unknown.c.view().notice));TEST_ASSERT_FALSE(unknown.c.take(out));
}
static void pending_idle_stop_and_back_hold_cancellation_are_explicit() {
  Drive d;d.devices();d.f.base.target.moving=false;Command out;
  d.click();TEST_ASSERT_FALSE(d.c.take(out));TEST_ASSERT_EQUAL(int(Notice::idle),int(d.c.view().notice));
  d.f.base.controlPending=true;d.click();TEST_ASSERT_TRUE(d.c.take(out));TEST_ASSERT_EQUAL(int(Action::stop),int(out.action));
  d.back();TEST_ASSERT_TRUE(d.c.takeCancellation());TEST_ASSERT_FALSE(d.c.takeCancellation());TEST_ASSERT_FALSE(d.c.take(out));
  Drive held;held.devices();held.tick(0,true);held.tick(0,true);held.tick(0,true,true,false,850);held.tick();held.tick();
  TEST_ASSERT_TRUE(held.c.takeCancellation());TEST_ASSERT_FALSE(held.c.take(out));
  d.c.result(d.now,DispatchResult::expired);TEST_ASSERT_EQUAL(int(Notice::expired),int(d.c.view().notice));
  d.c.result(d.now,DispatchResult::cancelledBusy);TEST_ASSERT_EQUAL(int(Notice::stopNotQueued),int(d.c.view().notice));
}
static void assertTransientDiscarded(Drive &d) {
  d.c.discardTransient();
  const auto &view=d.c.view();
  TEST_ASSERT_EQUAL(int(Screen::status),int(view.screen));TEST_ASSERT_EQUAL(0,view.entity.id);
  TEST_ASSERT_EQUAL(0,view.openedToken);TEST_ASSERT_FALSE(view.authorizedSelection);TEST_ASSERT_EQUAL(0,view.selectionAuthGeneration);
  TEST_ASSERT_EQUAL(0,view.credentialLength);TEST_ASSERT_EQUAL(0,view.credentialMaximum);TEST_ASSERT_EQUAL(0,view.credentialChoice);
  TEST_ASSERT_EQUAL(0,view.draftValue);TEST_ASSERT_EQUAL(0,view.valueChoice);
  Command command;Credentials credentials;DisplayPreferences preferences;FormInput input;
  TEST_ASSERT_FALSE(d.c.take(command));TEST_ASSERT_FALSE(d.c.takeCredentials(credentials));
  TEST_ASSERT_FALSE(d.c.takePreferences(preferences));TEST_ASSERT_FALSE(d.c.takeFormInput(input));
  TEST_ASSERT_FALSE(d.c.takeActivity());TEST_ASSERT_FALSE(d.c.takeCancellation());
}
static void graphics_discard_clears_raw_and_submitted_credentials_value_and_command() {
  for(unsigned submit=0;submit<2;submit++) {
    Drive d;d.f.base.authorized=false;d.f.base.authMode=AuthMode::pin;d.devices();d.click();d.tick(1);d.click();
    TEST_ASSERT_EQUAL(1,d.c.view().credentialLength);
    if(submit) {for(unsigned i=0;i<3;i++)d.click();for(unsigned i=0;i<96;i++)d.tick(1);d.click();}
    assertTransientDiscarded(d);
  }
  Drive value;openMore(value);value.click();value.tick(1);TEST_ASSERT_EQUAL(36,value.c.view().draftValue);assertTransientDiscarded(value);
  Drive command;command.devices();command.tick(-1);command.click();assertTransientDiscarded(command);
}
static void graphics_discard_clears_settings_events_and_preserves_live_preferences() {
  Drive events;events.c.setPreferences({128,30,60});events.home();for(unsigned i=0;i<3;i++)events.tick(1);events.click();
  events.tick(1);events.click();events.back();assertTransientDiscarded(events);
  const auto preserved=events.c.preferences();TEST_ASSERT_EQUAL(128,preserved.brightness);TEST_ASSERT_EQUAL(30,preserved.dimSeconds);TEST_ASSERT_EQUAL(60,preserved.offSeconds);
  for(unsigned phase=0;phase<4;phase++) {
    Drive d;d.home();for(unsigned i=0;i<4;i++)d.tick(1);d.click();d.click();d.tick(1); //32->38 live preview
    if(phase) {d.click();for(unsigned i=0;i<3;i++)d.tick(1);d.click();} // explicit Save produced, but not yet consumed
    if(phase>=2) {DisplayPreferences output;TEST_ASSERT_TRUE(d.c.takePreferences(output));d.c.brightnessResult(d.now,phase==2);}
    assertTransientDiscarded(d);
    TEST_ASSERT_EQUAL(phase<2?32:38,d.c.preference()); // consumed failed Save also keeps its already applied live value
  }
}
static void graphics_discard_restarts_debounce_and_suppresses_held_recovery_input() {
  for(unsigned back=0;back<2;back++) {
    Drive d;d.devices();d.tick(0,!back,true,back);d.tick(0,!back,true,back);
    assertTransientDiscarded(d);
    d.tick(0,!back,true,back);d.tick(0,!back,true,back,900);d.tick();d.tick();
    TEST_ASSERT_EQUAL(int(Screen::status),int(d.c.view().screen));
    Command command;TEST_ASSERT_FALSE(d.c.take(command));TEST_ASSERT_FALSE(d.c.takeCancellation());TEST_ASSERT_FALSE(d.c.takeActivity());
    d.click();TEST_ASSERT_EQUAL(int(Screen::home),int(d.c.view().screen)); // fresh gesture works after release
  }
}
static void status_hold_details_scroll_reaches_health_bottom_without_commands() {
  Drive d;d.tick();d.tick(0,true);d.tick(0,true);d.tick(0,true,true,false,850);d.tick();d.tick();
  TEST_ASSERT_EQUAL(Screen::details,d.c.view().screen);
  for(unsigned i=0;i<40;i++) d.tick(1);
  TEST_ASSERT_EQUAL(detailsRowCount-detailsVisibleRows,d.c.view().detailsOffset);
  TEST_ASSERT_GREATER_OR_EQUAL(14,detailsRowCount);
  Command command;TEST_ASSERT_FALSE(d.c.take(command));
  d.back();TEST_ASSERT_EQUAL(Screen::status,d.c.view().screen);TEST_ASSERT_FALSE(d.c.take(command));
}
static void openRepeats(Drive &d,Action action) {
  auto &descriptor=d.f.base.target.descriptor;descriptor.paired=true;descriptor.bitLength=80;
  openMore(d);
  for(unsigned i=0;i<20 && d.c.view().action!=repeatsNavigation;i++) d.tick(1);
  d.click();TEST_ASSERT_EQUAL(Screen::repeatActions,d.c.view().screen);
  for(unsigned i=0;i<20 && d.c.view().action!=action;i++) d.tick(1);
  TEST_ASSERT_EQUAL(action,d.c.view().action);d.click();
}
static void repeat_final_apply_tap_never_transmits() {
 Drive d;openRepeats(d,Action::up);d.click();d.tick(-1);
 TEST_ASSERT_EQUAL(1,d.c.view().valueChoice);
 d.click();Command out;TEST_ASSERT_FALSE(d.c.take(out));
 TEST_ASSERT_EQUAL(Screen::repeatValue,d.c.view().screen);
}
static void repeat_hold_binds_raw_press_and_release_never_becomes_stop() {
 Command out;
 Drive d;openRepeats(d,Action::up);d.click();d.tick(-1);
 d.tick(0,true);d.tick(0,true);d.tick(0,true,true,false,799);
 TEST_ASSERT_FALSE(d.c.take(out));
 d.tick(0,true,true,false,1);TEST_ASSERT_TRUE(d.c.take(out));
 TEST_ASSERT_EQUAL(1,out.repeat);TEST_ASSERT_TRUE(d.c.view().repeatHeld);
 d.c.result(d.now,DispatchResult::queued);TEST_ASSERT_TRUE(d.c.view().repeatHeld);
 d.c.result(d.now,DispatchResult::done);TEST_ASSERT_FALSE(d.c.view().repeatHeld);
 TEST_ASSERT_EQUAL(Notice::done,d.c.view().notice);d.tick(0,true,true,false,1500);
 TEST_ASSERT_FALSE(d.c.take(out)); // finite maximum completed while held
 d.tick(0,false);TEST_ASSERT_FALSE(d.c.take(out));
 d.tick();TEST_ASSERT_FALSE(d.c.take(out));TEST_ASSERT_FALSE(d.c.view().repeatHeld);
 TEST_ASSERT_EQUAL(Action::stop,d.c.view().action);
 // Even failed admission consumes the physical hold through release.
 Drive refused;openRepeats(refused,Action::up);refused.click();refused.tick(-1);refused.holdSend();
 TEST_ASSERT_TRUE(refused.c.take(out));refused.c.result(refused.now,DispatchResult::refused);
 TEST_ASSERT_FALSE(refused.c.view().repeatHeld);TEST_ASSERT_EQUAL(Notice::refused,refused.c.view().notice);
 refused.tick(0,true,true,false,1000);TEST_ASSERT_FALSE(refused.c.take(out));
 refused.tick();refused.tick();TEST_ASSERT_FALSE(refused.c.take(out));
 Drive expired;openRepeats(expired,Action::up);expired.click();expired.tick(-1);expired.holdSend();
 TEST_ASSERT_TRUE(expired.c.take(out));expired.c.result(expired.now,DispatchResult::expired);
 TEST_ASSERT_FALSE(expired.c.view().repeatHeld);TEST_ASSERT_EQUAL(Notice::expired,expired.c.view().notice);
 expired.tick(0,true,true,false,1000);TEST_ASSERT_FALSE(expired.c.take(out));
 expired.tick();expired.tick();TEST_ASSERT_FALSE(expired.c.take(out));
 // Admission remains active: a queued result must keep release cancellation.
 Drive queued;openRepeats(queued,Action::up);queued.click();queued.tick(-1);queued.holdSend();
 TEST_ASSERT_TRUE(queued.c.take(out));queued.c.result(queued.now,DispatchResult::queued);
 TEST_ASSERT_TRUE(queued.c.view().repeatHeld);queued.tick(0,false);
 TEST_ASSERT_TRUE(queued.c.takeCancellation());queued.tick();TEST_ASSERT_FALSE(queued.c.take(out));
 for(unsigned changed=0;changed<12;++changed) {
  Drive x;openRepeats(x,Action::stepDown);x.click();x.tick(-1);x.click(); // step editor -> count
  x.click();x.tick(-1);TEST_ASSERT_EQUAL(Screen::repeatValue,x.c.view().screen);
  x.tick(0,true,changed!=0);
  if(changed==1)++x.f.base.target.token;
  if(changed==2)x.f.base.authorized=false;
  if(changed==3)++x.f.base.authGeneration;
  if(changed==4)++x.f.base.authConfigurationGeneration;
  if(changed==5)++x.f.base.modelRevision;
  if(changed==6)++x.f.base.restoreRevision;
  if(changed==7)x.f.base.ready=false;
  if(changed==8)x.f.base.controlsEnabled=false;
  if(changed==9)x.f.base.controlPending=true;
  if(changed==10)x.now-=1000;
  x.tick(changed==11?1:0,true,changed!=0);
  // A busy SPI finishing after an ineligible raw press cannot arm it later.
  x.tick(0,true,true,false,1000);TEST_ASSERT_FALSE(x.c.take(out));
  x.tick();x.tick();TEST_ASSERT_FALSE(x.c.take(out));
 }
 Drive lateFrame;openRepeats(lateFrame,Action::up);lateFrame.click();lateFrame.tick(-1);
 lateFrame.tick(0,true,false); // raw press sees an incomplete reviewed frame
 lateFrame.tick(0,true,true); // SPI completes before the debounce transition
 lateFrame.tick(0,true,true,false,1000);TEST_ASSERT_FALSE(lateFrame.c.take(out));
 lateFrame.tick();lateFrame.tick();TEST_ASSERT_FALSE(lateFrame.c.take(out));
 lateFrame.holdSend();TEST_ASSERT_TRUE(lateFrame.c.take(out)); // fresh press may qualify
 Drive obscured;openRepeats(obscured,Action::up);obscured.click();obscured.tick(-1);
 obscured.tick(0,true);obscured.tick(0,true,false);obscured.tick(0,true,true,false,1000);
 TEST_ASSERT_FALSE(obscured.c.take(out));
}
static void repeats_are_bounded_explicit_presented_and_cancel_without_effects() {
  Drive d;openRepeats(d,Action::up);TEST_ASSERT_EQUAL(Screen::repeatValue,d.c.view().screen);
  Command out;for(unsigned i=0;i<140;i++) d.tick(1);
  TEST_ASSERT_EQUAL(127,d.c.view().draftValue);TEST_ASSERT_FALSE(d.c.take(out));
  d.click();TEST_ASSERT_EQUAL(2,d.c.view().valueChoice);d.tick(-1);d.click(false);TEST_ASSERT_FALSE(d.c.take(out));
  d.holdSend();TEST_ASSERT_TRUE(d.c.take(out));TEST_ASSERT_EQUAL(127,out.repeat);TEST_ASSERT_EQUAL(Action::up,out.action);
  TEST_ASSERT_EQUAL(99,out.token);TEST_ASSERT_EQUAL(Action::stop,d.c.view().action);
  Drive step;openRepeats(step,Action::stepDown);TEST_ASSERT_EQUAL(Screen::value,step.c.view().screen);
  step.tick(1);step.tick(1);step.click();step.tick(-1);step.click();
  TEST_ASSERT_EQUAL(Screen::repeatValue,step.c.view().screen);TEST_ASSERT_FALSE(step.c.take(out));
  step.tick(1);step.click();step.tick(-1);step.holdSend();TEST_ASSERT_TRUE(step.c.take(out));
  TEST_ASSERT_EQUAL(3,out.step);TEST_ASSERT_EQUAL(2,out.repeat);TEST_ASSERT_EQUAL(Action::stepDown,out.action);
  for(unsigned reason=0;reason<5;reason++) {
    Drive cancelled;openRepeats(cancelled,Action::up);cancelled.tick(1);
    if(reason==0) cancelled.back();
    if(reason==1) {cancelled.click();cancelled.click();}
    if(reason==2) {++cancelled.f.base.target.token;cancelled.tick();}
    if(reason==3) {cancelled.f.base.authorized=false;cancelled.tick();}
    if(reason==4) cancelled.tick(0,false,true,false,30000);
    TEST_ASSERT_FALSE(cancelled.c.take(out));TEST_ASSERT_EQUAL(Screen::control,cancelled.c.view().screen);
    TEST_ASSERT_EQUAL(Action::stop,cancelled.c.view().action);
  }
}
static void quick_home_only_navigates_and_keeps_browse_and_drafts_separate() {
 Drive d;d.f.base.anyMoving=false;d.f.base.quickReady=true;d.f.base.quickRevision=3;
 d.f.groups[0]={1,0,0,"Both",500,2,-1,false,true};
 d.f.base.quickAccess.assign(0,{EntityKind::group,1},500);
 d.f.base.target.ref={EntityKind::group,1};d.f.base.target.identity=500;d.f.base.target.revision=2;
 d.tick();TEST_ASSERT_EQUAL(Screen::quickHome,d.c.view().screen);
 Command command;d.click();d.tick();TEST_ASSERT_EQUAL(Screen::control,d.c.view().screen);
 TEST_ASSERT_TRUE(d.c.view().quickOrigin);TEST_ASSERT_EQUAL(Action::stop,d.c.view().action);TEST_ASSERT_FALSE(d.c.take(command));
 d.back();TEST_ASSERT_EQUAL(Screen::quickHome,d.c.view().screen);
 for(unsigned i=0;i<4;i++)d.tick(1);d.click();TEST_ASSERT_EQUAL(Screen::catalogue,d.c.view().screen);
 d.back();TEST_ASSERT_EQUAL(Screen::quickHome,d.c.view().screen);
 d.tick(1);d.manageQuick();TEST_ASSERT_TRUE(d.c.view().quickEditing);
 d.click();TEST_ASSERT_EQUAL(Screen::quickActions,d.c.view().screen);
 d.tick(1);d.tick(1);d.click();TEST_ASSERT_TRUE(d.c.view().quickDraft.at(0)->empty());
 TEST_ASSERT_FALSE(d.f.base.quickAccess.at(0)->empty());
 d.tick(1);d.tick(1);d.tick(1);d.click();TEST_ASSERT_EQUAL(Screen::quickSave,d.c.view().screen);
 TEST_ASSERT_EQUAL(0,d.c.view().valueChoice);QuickPreferenceRequest save;TEST_ASSERT_FALSE(d.c.takeQuickSave(save));
 d.tick(1);d.click();TEST_ASSERT_TRUE(d.c.takeQuickSave(save));TEST_ASSERT_TRUE(save.value.at(0)->empty());
 TEST_ASSERT_EQUAL_UINT32(2,save.authGeneration);TEST_ASSERT_FALSE(d.c.take(command));
 d.c.quickSaveResult(d.now,QuickPreferenceSave::unknown);TEST_ASSERT_EQUAL(Notice::quickSaveUnknown,d.c.view().notice);
 d.back();d.back();TEST_ASSERT_FALSE(d.c.view().quickEditing);TEST_ASSERT_FALSE(d.c.view().quickPicking);
 TEST_ASSERT_TRUE(d.c.view().quickDraft.at(0)->empty());
}
static void quick_revocation_consumes_only_an_existing_press() {
 for(bool held:{false,true}) {
  Drive d;d.f.base.anyMoving=false;d.f.base.quickReady=true;d.f.base.quickRevision=3;d.tick();
  d.manageQuick();
  if(held){d.tick(0,true);d.tick(0,true);}
  ++d.f.base.modelRevision;d.tick(0,held);
  TEST_ASSERT_EQUAL(Screen::quickHome,d.c.view().screen);TEST_ASSERT_FALSE(d.c.view().quickEditing);
  QuickPreferenceRequest save;Command command;
  if(held) {
   d.tick(0,true,true,false,900);TEST_ASSERT_EQUAL(Screen::quickHome,d.c.view().screen);
   d.tick();d.tick();TEST_ASSERT_EQUAL(Screen::quickHome,d.c.view().screen);
  }
  TEST_ASSERT_FALSE(d.c.takeQuickSave(save));TEST_ASSERT_FALSE(d.c.take(command));
  d.click();TEST_ASSERT_EQUAL(Screen::quickManage,d.c.view().screen);TEST_ASSERT_TRUE(d.c.view().quickEditing);
 }
}
static void approved_controller_navigation_preserves_explicit_origins(){
 Drive d;d.f.base.quickReady=true;d.f.base.quickRevision=3;d.f.base.anyMoving=false;d.tick();
 for(unsigned i=0;i<7;i++)d.tick(1);d.click();TEST_ASSERT_EQUAL(Screen::controllerMenu,d.c.view().screen);
 Command command;TEST_ASSERT_FALSE(d.c.take(command));d.click();TEST_ASSERT_EQUAL(Screen::configureMenu,d.c.view().screen);
 d.click();TEST_ASSERT_EQUAL(Screen::settings,d.c.view().screen);TEST_ASSERT_EQUAL(0,d.c.view().settingsSection);d.c.closeForm();TEST_ASSERT_EQUAL(Screen::configureMenu,d.c.view().screen);TEST_ASSERT_EQUAL(0,d.c.view().selected);
 d.tick(1);d.tick(1);d.click();TEST_ASSERT_EQUAL(Screen::networkMenu,d.c.view().screen);d.tick(1);d.click();TEST_ASSERT_EQUAL(Screen::system,d.c.view().screen);TEST_ASSERT_EQUAL(3,d.c.view().systemSection);d.c.closeForm();TEST_ASSERT_EQUAL(Screen::networkMenu,d.c.view().screen);TEST_ASSERT_EQUAL(1,d.c.view().selected);
 d.back();d.back();d.tick(1);d.click();TEST_ASSERT_EQUAL(1,d.c.view().systemSection);d.c.closeForm();TEST_ASSERT_EQUAL(Screen::controllerMenu,d.c.view().screen);TEST_ASSERT_EQUAL(1,d.c.view().selected);
 d.tick(1);d.tick(1);d.tick(1);d.click();TEST_ASSERT_EQUAL(Screen::system,d.c.view().screen);TEST_ASSERT_EQUAL(2,d.c.view().systemSection);d.c.openSettingsFromSystem();TEST_ASSERT_EQUAL(Screen::settings,d.c.view().screen);d.c.closeForm();TEST_ASSERT_EQUAL(Screen::system,d.c.view().screen);TEST_ASSERT_EQUAL(2,d.c.view().systemSelection);d.c.closeForm();TEST_ASSERT_EQUAL(Screen::controllerMenu,d.c.view().screen);d.tick(-1);d.click();TEST_ASSERT_EQUAL(Screen::details,d.c.view().screen);d.back();TEST_ASSERT_EQUAL(Screen::controllerMenu,d.c.view().screen);TEST_ASSERT_EQUAL(3,d.c.view().selected);TEST_ASSERT_FALSE(d.c.take(command));
}
static void routed_display_revocation_returns_to_its_visible_origin(){
 for(unsigned cause=0;cause<4;++cause){
  Drive d;d.f.base.quickReady=true;d.f.base.quickRevision=3;d.f.base.anyMoving=false;d.tick();
  for(unsigned i=0;i<7;i++)d.tick(1);d.click();d.click();for(unsigned i=0;i<5;i++)d.tick(1);d.click();TEST_ASSERT_EQUAL(Screen::brightness,d.c.view().screen);
  const bool held=cause&1;if(held){d.tick(0,true);d.tick(0,true);}
  if(cause&2)d.f.base.authorized=false;else ++d.f.base.authConfigurationGeneration;
  d.tick(0,held);TEST_ASSERT_EQUAL(Screen::configureMenu,d.c.view().screen);TEST_ASSERT_EQUAL(5,d.c.view().selected);
  if(held){d.tick(0,true,true,false,900);d.tick();d.tick();TEST_ASSERT_EQUAL(Screen::configureMenu,d.c.view().screen);}
  DisplayPreferences prefs;Command command;TEST_ASSERT_FALSE(d.c.takePreferences(prefs));TEST_ASSERT_FALSE(d.c.take(command));
  d.click();TEST_ASSERT_EQUAL(cause&2?Screen::unlock:Screen::brightness,d.c.view().screen);
 }
}
static void routed_display_save_returns_to_configure_once(){
 Drive d;d.f.base.quickReady=true;d.f.base.quickRevision=3;d.f.base.anyMoving=false;d.tick();
 for(unsigned i=0;i<7;i++)d.tick(1);d.click();d.click();for(unsigned i=0;i<5;i++)d.tick(1);d.click();
 TEST_ASSERT_EQUAL(Screen::brightness,d.c.view().screen);for(unsigned i=0;i<3;i++)d.tick(1);d.click();
 TEST_ASSERT_EQUAL(Screen::configureMenu,d.c.view().screen);TEST_ASSERT_EQUAL(5,d.c.view().selected);
 DisplayPreferences prefs;Command command;TEST_ASSERT_TRUE(d.c.takePreferences(prefs));TEST_ASSERT_FALSE(d.c.takePreferences(prefs));TEST_ASSERT_FALSE(d.c.take(command));
}
static void upper_brightness_changes_in_five_percent_steps_without_losing_saved_levels() {
  Drive d; d.c.setPreferences({128,0,0});d.home();for(unsigned i=0;i<4;i++)d.tick(1);d.click();
  TEST_ASSERT_EQUAL(Screen::brightness,d.c.view().screen);d.click();
  d.tick(1); TEST_ASSERT_EQUAL(55,(d.c.preference()*100+127)/255);
  d.tick(-1);TEST_ASSERT_EQUAL(50,(d.c.preference()*100+127)/255);
  for(unsigned i=0;i<10;i++)d.tick(1);
  TEST_ASSERT_EQUAL(100,(d.c.preference()*100+127)/255);
  for(unsigned i=0;i<18;i++)d.tick(-1);
  TEST_ASSERT_EQUAL(10,(d.c.preference()*100+127)/255);
  d.back();TEST_ASSERT_EQUAL(128,d.c.preference());
  d.c.setPreferences({32,0,0}); TEST_ASSERT_EQUAL(32,d.c.preference());
}
static void unseen_unlock_key_never_appends_until_presented() {
  Drive d;d.f.base.authorized=false;d.f.base.authMode=AuthMode::pin;d.devices();d.click();
  TEST_ASSERT_EQUAL(Screen::unlock,d.c.view().screen);
  d.tick(1);d.click(false);
  TEST_ASSERT_EQUAL(0,d.c.view().credentialLength);
  Credentials credentials;TEST_ASSERT_FALSE(d.c.takeCredentials(credentials));
  d.click();TEST_ASSERT_EQUAL(1,d.c.view().credentialLength);
  Command command;TEST_ASSERT_FALSE(d.c.take(command));
}
static void unseen_unlock_done_never_submits_until_presented() {
  Drive d;d.f.base.authorized=false;d.f.base.authMode=AuthMode::pin;d.devices();d.click();
  d.tick(1);for(unsigned i=0;i<4;i++)d.click();
  const auto done=RotaryKeyboard::actionOffset(d.c.view().credentialKeyboard)+5;
  while(d.c.view().credentialKeyboard.selected!=done)d.tick(1);
  d.click(false);Credentials credentials;TEST_ASSERT_FALSE(d.c.takeCredentials(credentials));
  TEST_ASSERT_EQUAL(4,d.c.view().credentialLength);
  d.click();TEST_ASSERT_TRUE(d.c.takeCredentials(credentials));
  TEST_ASSERT_EQUAL_STRING("1111",credentials.secret);
  Command command;TEST_ASSERT_FALSE(d.c.take(command));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(unseen_unlock_key_never_appends_until_presented);
  RUN_TEST(unseen_unlock_done_never_submits_until_presented);
  RUN_TEST(display_cancel_timeout_and_reentry_discard_edits);
  RUN_TEST(display_wheel_selects_rows_and_only_explicit_save_persists);
  RUN_TEST(upper_brightness_changes_in_five_percent_steps_without_losing_saved_levels);
  RUN_TEST(routed_display_save_returns_to_configure_once);
  RUN_TEST(routed_display_revocation_returns_to_its_visible_origin);
  RUN_TEST(approved_controller_navigation_preserves_explicit_origins);
  RUN_TEST(quick_revocation_consumes_only_an_existing_press);
  RUN_TEST(quick_home_only_navigates_and_keeps_browse_and_drafts_separate);
  RUN_TEST(repeat_hold_binds_raw_press_and_release_never_becomes_stop);
  RUN_TEST(repeat_final_apply_tap_never_transmits);
  RUN_TEST(repeats_are_bounded_explicit_presented_and_cancel_without_effects);
  RUN_TEST(administration_requires_unlock_and_forwards_only_presented_form_press);
  RUN_TEST(status_hold_details_scroll_reaches_health_bottom_without_commands);
  RUN_TEST(stable_catalogue_selection_binds_id_not_slot);
  RUN_TEST(never_presented_catalogue_or_stop_emits_no_intent);
  RUN_TEST(empty_full_and_live_reordered_catalogues_are_bounded);
  RUN_TEST(target_replacement_between_row_and_focus_is_not_rebound);
  RUN_TEST(open_context_change_does_not_reseed_or_retarget);
  RUN_TEST(locked_selection_unlocks_without_replaying);
  RUN_TEST(locked_stop_lease_never_authorizes_another_action);
  RUN_TEST(secrets_wipe_on_cancel_configuration_change_sleep_and_timeout);
  RUN_TEST(wake_holds_and_mixed_gestures_do_not_emit);
  RUN_TEST(brightness_back_cancels_and_explicit_save_reports_failure);
  RUN_TEST(privileged_brightness_draft_reverts_on_expiry_or_configuration_change);
  RUN_TEST(settings_receives_logical_events_and_closes_explicitly);
  RUN_TEST(encoder_detents_bounce_and_memory_failures_remain_bounded);
  RUN_TEST(favourite_requires_human_observation_then_separate_displayed_confirmation);
  RUN_TEST(favourite_observation_cannot_survive_motion_expiry_or_context_change);
  RUN_TEST(numeric_control_draft_requires_displayed_apply_and_cancel_never_emits);
  RUN_TEST(numeric_control_drafts_discard_on_context_authorization_and_timeout);
  RUN_TEST(toggle_and_capability_menus_have_no_invisible_fallback);
  RUN_TEST(step_bounds_and_tilt_stop_never_borrow_stop_grace);
  RUN_TEST(pending_idle_stop_and_back_hold_cancellation_are_explicit);
  RUN_TEST(graphics_discard_clears_raw_and_submitted_credentials_value_and_command);
  RUN_TEST(graphics_discard_clears_settings_events_and_preserves_live_preferences);
  RUN_TEST(graphics_discard_restarts_debounce_and_suppresses_held_recovery_input);
  return UNITY_END();
}

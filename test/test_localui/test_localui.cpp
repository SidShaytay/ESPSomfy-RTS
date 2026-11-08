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
      cataloguePage(*this,v.catalogue,v.cursor,v.room,m.page,v.anchorId,v.anchored);
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
  void click(bool visible=true) { tick(0,true,visible);tick(0,true,visible);tick(0,false,visible);tick(0,false,visible); }
  void back() { tick(0,false,true,true);tick(0,false,true,true);tick();tick(); }
  void home() { tick(); tick(1); }
  void devices(bool visible=true) { home(); tick(1); click(); click(visible); tick(); }
};
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
  d.f.base.authorized=false; d.f.base.target.stopLease=true; d.click();
  TEST_ASSERT_FALSE(d.c.take(out)); TEST_ASSERT_EQUAL(int(Notice::changed),int(d.c.view().notice));
}
static void locked_selection_unlocks_without_replaying() {
  Drive d; d.f.base.authorized=false; d.f.base.authMode=AuthMode::pin; d.devices(); d.click();
  TEST_ASSERT_EQUAL(int(Screen::unlock),int(d.c.view().screen));
  d.tick(1); for(unsigned i=0;i<4;i++) d.click();
  for(unsigned i=0;i<96;i++) d.tick(1);
  d.click(); Credentials given; TEST_ASSERT_TRUE(d.c.takeCredentials(given));
  TEST_ASSERT_EQUAL_STRING("!!!!",given.secret);
  d.c.unlockResult(d.now,AuthResult::unlocked);
  d.f.base.authorized=true; ++d.f.base.authGeneration; d.tick();
  TEST_ASSERT_TRUE(d.c.view().authorizedSelection);
  Command out; TEST_ASSERT_FALSE(d.c.take(out));
  d.click(); TEST_ASSERT_TRUE(d.c.take(out));
}
static void locked_stop_lease_never_authorizes_another_action() {
  Drive d; d.devices(); d.f.base.authorized=false; d.f.base.target.stopLease=true;
  d.click(); Command out; TEST_ASSERT_TRUE(d.c.take(out));
  d.tick(-1); d.click(); TEST_ASSERT_FALSE(d.c.take(out));
  TEST_ASSERT_EQUAL(int(Screen::unlock),int(d.c.view().screen));
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
static void brightness_back_cancels_and_explicit_save_reports_failure() {
  Drive d; d.home(); for(unsigned i=0;i<4;i++) d.tick(1); d.click();
  TEST_ASSERT_EQUAL(int(Screen::brightness),int(d.c.view().screen));
  d.tick(1); TEST_ASSERT_EQUAL(64,d.c.preference()); d.back();
  DisplayPreferences saved; TEST_ASSERT_FALSE(d.c.takePreferences(saved)); TEST_ASSERT_EQUAL(32,d.c.preference());
  d.click(); d.tick(1); d.click();d.click();d.click();
  TEST_ASSERT_TRUE(d.c.takePreferences(saved)); TEST_ASSERT_EQUAL(64,saved.brightness);
  d.c.brightnessResult(d.now,false); TEST_ASSERT_EQUAL(int(Notice::saveFailed),int(d.c.view().notice));
}
static void privileged_brightness_draft_reverts_on_expiry_or_configuration_change() {
  for(unsigned reason=0;reason<2;reason++) {
    Drive d; d.home();for(unsigned i=0;i<4;i++) d.tick(1);d.click();d.tick(1);
    if(reason==0) d.f.base.authorized=false; else ++d.f.base.authConfigurationGeneration;
    d.tick(1); TEST_ASSERT_NOT_EQUAL(int(Screen::brightness),int(d.c.view().screen));
    TEST_ASSERT_EQUAL(32,d.c.preference()); DisplayPreferences saved;TEST_ASSERT_FALSE(d.c.takePreferences(saved));
  }
}
static void settings_receives_logical_events_and_closes_explicitly() {
  Drive d;d.home(); for(unsigned i=0;i<3;i++) d.tick(1); d.click();
  TEST_ASSERT_EQUAL(int(Screen::settings),int(d.c.view().screen));
  SettingsInput event; TEST_ASSERT_FALSE(d.c.takeSettingsInput(event));
  d.tick(1);TEST_ASSERT_TRUE(d.c.takeSettingsInput(event));TEST_ASSERT_EQUAL(1,event.turn);
  d.click();TEST_ASSERT_TRUE(d.c.takeSettingsInput(event));TEST_ASSERT_TRUE(event.activate);
  d.back();TEST_ASSERT_TRUE(d.c.takeSettingsInput(event));TEST_ASSERT_TRUE(event.back);
  TEST_ASSERT_EQUAL(int(Screen::settings),int(d.c.view().screen));
  d.c.closeSettings(); TEST_ASSERT_EQUAL(int(Screen::home),int(d.c.view().screen));
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
  tilt.f.base.target.tiltMoving=true;tilt.f.base.authorized=false;tilt.f.base.target.stopLease=true;
  tilt.click();TEST_ASSERT_FALSE(tilt.c.take(out));TEST_ASSERT_EQUAL(int(Screen::unlock),int(tilt.c.view().screen));
  Drive unknown;openMore(unknown);unknown.f.base.target.position=-1;unknown.click();
  TEST_ASSERT_EQUAL(int(Screen::actions),int(unknown.c.view().screen));
  TEST_ASSERT_EQUAL(int(Notice::valueUnavailable),int(unknown.c.view().notice));TEST_ASSERT_FALSE(unknown.c.take(out));
}
int main() {
  UNITY_BEGIN();RUN_TEST(stable_catalogue_selection_binds_id_not_slot);
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
  RUN_TEST(numeric_control_draft_requires_displayed_apply_and_cancel_never_emits);
  RUN_TEST(numeric_control_drafts_discard_on_context_authorization_and_timeout);
  RUN_TEST(toggle_and_capability_menus_have_no_invisible_fallback);
  RUN_TEST(step_bounds_and_tilt_stop_never_borrow_stop_grace);
  return UNITY_END();
}

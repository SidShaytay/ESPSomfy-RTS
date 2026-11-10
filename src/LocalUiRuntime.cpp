#ifdef ESPSOMFY_LOCAL_UI
#include "LocalUiRuntime.h"
#include "LocalUiAdapter.h"
#include "LocalUiCapture.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LocalEntityRuntime.h"
#include "LocalAdvancedRuntime.h"
#include "BoardProfile.h"
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <string.h>
namespace LocalUi {
namespace {
uint32_t hash(const void *p,size_t n,uint32_t h=2166136261u) {
  const auto *b=static_cast<const uint8_t *>(p);while(n--) h=(h^*b++)*16777619u;return h;
}
uint32_t sceneHash(const Model &m,const View &v,const StatusDisplay::Snapshot &s) {
  const uint32_t values[]={uint32_t(v.screen),v.selected,v.cursor,uint32_t(v.catalogue),v.room,
    uint32_t(v.entity.kind),v.entity.id,uint32_t(v.openedToken),uint32_t(v.openedToken>>32),uint32_t(v.action),
    v.valueChoice,uint32_t(v.draftValue),uint32_t(v.notice),v.level,v.setting,v.detailsOffset,v.dimSeconds,v.offSeconds,
    v.credentialChoice,v.credentialLength,v.credentialMaximum,v.credentialUser,v.credentialByte,v.retryAfterMs/1000,
    m.authorized,m.authGeneration,m.authConfigurationGeneration,uint32_t(m.authMode),m.ready,m.controlsEnabled,m.controlPending,
    uint32_t(m.pageToken),uint32_t(m.pageToken>>32),m.page.total,m.page.cursor,m.target.exists,m.target.valid,
    uint32_t(m.target.token),uint32_t(m.target.token>>32),m.target.capabilities,m.target.moving,m.target.stopLease,
    uint32_t(m.target.position),uint32_t(m.target.tiltPosition),m.target.descriptor.memberCount,m.target.movingCount,
    uint32_t(m.target.descriptor.shadeType),uint32_t(m.target.descriptor.proto),m.target.descriptor.bitLength,
    s.running,uint32_t(s.battery.state),s.battery.percent,s.battery.millivolts,uint32_t(s.link),uint32_t(s.web),
    uint32_t(s.radio),uint32_t(s.frequency*100),uint32_t(v.screen==Screen::details?s.uptimeSeconds:s.uptimeSeconds/60)};
  uint32_t h=hash(values,sizeof(values));
  for(const auto &entry:m.page.rows) {
    const int32_t values[]={entry.id,entry.position,entry.available,entry.moving};
    h=hash(values,sizeof(values),h);h=hash(entry.name,strlen(entry.name),h);
  }
  h=hash(m.target.name,strlen(m.target.name),h);h=hash(v.roomName,strlen(v.roomName),h);
  h=hash(s.address,strlen(s.address),h);h=hash(s.build,strlen(s.build),h);
  h=hash(s.firmwareVersion,strlen(s.firmwareVersion),h);
  return hash(s.applicationVersion,strlen(s.applicationVersion),h);
}
uint32_t settingsHash(const Settings::FormView &view) {
  const uint32_t fields[]={uint32_t(view.mode),uint32_t(view.notice),uint32_t(view.restartReason),view.rowCount,view.selected,view.total,
    view.offset,view.protectedFrame,view.dirty};
  uint32_t h=hash(fields,sizeof(fields));
  h=hash(view.title,sizeof(view.title),h);h=hash(view.hint,sizeof(view.hint),h);
  for(const auto &row:view.rows) {
    const uint32_t flags[]={row.selected,row.readOnly,row.dirty};h=hash(flags,sizeof(flags),h);
    h=hash(row.label,sizeof(row.label),h);h=hash(row.value,sizeof(row.value),h);
  }
  return h;
}
bool sameSettings(const Settings::FormView &a,const Settings::FormView &b) {
  if(a.mode!=b.mode || a.notice!=b.notice || a.restartReason!=b.restartReason || a.rowCount!=b.rowCount || a.selected!=b.selected ||
     a.total!=b.total || a.offset!=b.offset || a.protectedFrame!=b.protectedFrame || a.dirty!=b.dirty ||
     memcmp(a.title,b.title,sizeof(a.title)) || memcmp(a.hint,b.hint,sizeof(a.hint))) return false;
  for(uint8_t i=0;i<3;i++) if(a.rows[i].selected!=b.rows[i].selected || a.rows[i].readOnly!=b.rows[i].readOnly ||
      a.rows[i].dirty!=b.rows[i].dirty || memcmp(a.rows[i].label,b.rows[i].label,sizeof(a.rows[i].label)) ||
      memcmp(a.rows[i].value,b.rows[i].value,sizeof(a.rows[i].value))) return false;
  return true;
}
bool sameAdministration(const EntityAdmin::FormView &a,const EntityAdmin::FormView &b) {
  if(a.kind!=b.kind || a.id!=b.id || a.mode!=b.mode || a.notice!=b.notice || a.rowCount!=b.rowCount ||
     a.selected!=b.selected || a.total!=b.total || a.offset!=b.offset || a.protectedFrame!=b.protectedFrame ||
     a.dirty!=b.dirty || memcmp(a.title,b.title,sizeof(a.title)) || memcmp(a.hint,b.hint,sizeof(a.hint)))return false;
  for(uint8_t i=0;i<3;i++)if(a.rows[i].selected!=b.rows[i].selected || a.rows[i].dirty!=b.rows[i].dirty ||
      memcmp(a.rows[i].label,b.rows[i].label,sizeof(a.rows[i].label)) || memcmp(a.rows[i].value,b.rows[i].value,sizeof(a.rows[i].value)))return false;
  return true;
}
uint32_t administrationHash(const EntityAdmin::FormView &view,uint64_t epoch) {
  const uint32_t values[]={uint32_t(view.kind),view.id,uint32_t(view.mode),uint32_t(view.notice),view.rowCount,
    view.selected,view.total,view.offset,view.protectedFrame,view.dirty,uint32_t(epoch),uint32_t(epoch>>32)};
  uint32_t h=hash(values,sizeof(values));h=hash(view.title,sizeof(view.title),h);h=hash(view.hint,sizeof(view.hint),h);
  for(const auto &row:view.rows) {
    const uint32_t flags[]={row.selected,row.dirty};h=hash(flags,sizeof(flags),h);
    h=hash(row.label,sizeof(row.label),h);h=hash(row.value,sizeof(row.value),h);
  }
  return h;
}
// Motion/estimate repainting is coalesced until a frame finishes. Structural
// page, target, action, editor and authorization changes supersede it promptly.
bool sameMeaning(const View &a,const View &b,uint64_t page,uint64_t framePage,uint64_t target,uint64_t frameTarget) {
  if(a.screen!=b.screen || a.notice!=b.notice) return false;
  if(a.screen==Screen::catalogue) return page==framePage && a.catalogue==b.catalogue && a.room==b.room && a.cursor==b.cursor;
  if(a.screen==Screen::control || a.screen==Screen::actions || a.screen==Screen::value) {
    const bool sameTarget=a.entity==b.entity && a.openedToken==b.openedToken && target==frameTarget && a.action==b.action;
    if(a.screen==Screen::actions) return sameTarget && a.selected==b.selected;
    if(a.screen==Screen::value) return sameTarget && a.valueChoice==b.valueChoice && a.draftValue==b.draftValue;
    return sameTarget;
  }
  if(a.screen==Screen::unlock) return a.credentialChoice==b.credentialChoice && a.credentialLength==b.credentialLength &&
    a.credentialMaximum==b.credentialMaximum && a.credentialUser==b.credentialUser && a.credentialByte==b.credentialByte;
  if(a.screen==Screen::brightness) return a.level==b.level && a.dimSeconds==b.dimSeconds && a.offSeconds==b.offSeconds && a.setting==b.setting;
  if(a.screen==Screen::details) return a.detailsOffset==b.detailsOffset;
  return a.selected==b.selected && a.room==b.room;
}
void *allocate(size_t size){return heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
uint16_t transfer[screenWidth*bandHeight];
}
Runtime::Runtime():administration(EntityAdmin::runtimeBackend(),runtimeCatalogue(),Settings::runtimeMemory(),Advanced::runtimeBackend()) {}
void Runtime::begin() {
  setCaptureProtected(true);controller.setPreferences(loadPreferences());
  pinMode(BoardProfile::backButton,INPUT_PULLUP);
  memory.begin(heap_caps_get_total_size(MALLOC_CAP_SPIRAM)>0,allocate);
  if(memory.pixels) {canvas.attach(memory.pixels);pinMode(BoardProfile::encoderA,INPUT_PULLUP);pinMode(BoardProfile::encoderB,INPUT_PULLUP);}
}
bool Runtime::tick(uint32_t now,const StatusDisplay::Snapshot &s,bool pressed,
                   bool (*sendBand)(uint8_t,uint8_t,const uint8_t *,size_t),void (*light)(uint8_t)) {
  if(!memory.pixels) {
    if(!graphicsMissing) {
      graphicsMissing=true;
      settingsForm.discard();settingsActive=false;
      administration.discard();(void)administration.takePrivateProjectionDiscard();administrationActive=presentedAdministrationValid=false;
      discardSystemChild();systemForm.discard();systemActive=false;
      controller.discardTransient();
      cancelUnsubmittedLocalControl();
      DispatchResult ignored;takeLocalControlResult(ignored);
      invalidateControlPresentation();lockAuthorization();
      encoder=Encoder{};wasSleeping=false;
      frameSettings={};presentedSettings={};presentedSettingsValid=false;
      frameView={};presentedView={};stopPresentedRef={};
      frameTargetToken=presentedTargetToken=framePageToken=presentedPageToken=stopPresentedToken=0;
      validBands=0;memset(bandHashes,0,sizeof(bandHashes));
      scene=lastBand=0;bandIndex=43;attempted=hasPresented=false;
      canvas.attach(nullptr);
    }
    setCaptureProtected(false);capturePoll(nullptr,true,0,0);return false;
  }
  if(graphicsMissing) {
    graphicsMissing=false;canvas.attach(memory.pixels);setCaptureProtected(true);
  }
  const uint64_t settingsNow=uint64_t(esp_timer_get_time())/1000;
  const Model model=snapshotModel(s.running,controller.view());
  DispatchResult completion;
  if(takeLocalControlResult(completion)) controller.result(now,completion);
  EntityAdmin::FormView systemView; // 352-byte loop stack view; no additional retained frame.
  if(systemActive)renderSystem(systemView);
  const View &before=controller.view();
  bool visible=hasPresented && presentedView.screen==before.screen;
  if(before.screen==Screen::home)visible &= before.selected==presentedView.selected;
  else if(before.screen==Screen::administration)
    visible &= administrationActive && presentedAdministrationValid &&
      presentedAdministrationEpoch==administration.epoch() && sameAdministration(administration.view(),presentedAdministration);
  else if(before.screen==Screen::system)
    visible &= systemActive && presentedAdministrationValid && sameAdministration(systemView,presentedAdministration);
  else if(before.screen==Screen::catalogue)
    visible &= model.pageToken && presentedPageToken==model.pageToken && before.cursor==model.page.cursor &&
      presentedView.cursor==model.page.cursor && presentedView.catalogue==before.catalogue && presentedView.room==before.room;
  else if(before.screen==Screen::control) {
    const bool same=model.target.ref==before.entity && before.openedToken && model.target.token==before.openedToken;
    if(before.action==Action::stop) visible=same && stopPresentedRef==before.entity && stopPresentedToken==before.openedToken;
    else visible &= same && presentedView.entity==before.entity && presentedView.openedToken==before.openedToken &&
      presentedTargetToken==before.openedToken && presentedView.action==before.action;
  } else if(before.screen==Screen::actions || before.screen==Screen::value) {
    visible &= model.target.ref==before.entity && before.openedToken && model.target.token==before.openedToken &&
      presentedView.entity==before.entity && presentedView.openedToken==before.openedToken &&
      presentedTargetToken==before.openedToken && presentedView.action==before.action;
    if(before.screen==Screen::actions) visible &= presentedView.selected==before.selected;
    else visible &= presentedView.valueChoice==before.valueChoice && presentedView.draftValue==before.draftValue;
  } else if(before.screen==Screen::settings)
    visible &= settingsActive && presentedSettingsValid && sameSettings(settingsForm.view(),presentedSettings);
  else if(before.screen==Screen::brightness)
    visible &= before.setting==presentedView.setting && before.level==presentedView.level &&
      before.dimSeconds==presentedView.dimSeconds && before.offSeconds==presentedView.offSeconds;
  const int8_t turn=encoder.sample((digitalRead(BoardProfile::encoderA)<<1)|digitalRead(BoardProfile::encoderB));
  controller.tick(now,model,turn,pressed,visible,digitalRead(BoardProfile::backButton)==0);
  if(controller.takeCancellation()) cancelUnsubmittedLocalControl();
  DisplayPreferences preferences;
  if(controller.takePreferences(preferences)) controller.brightnessResult(now,model.authorized && savePreferences(preferences));
  if(controller.takeActivity()) authorizationActivity();
  Credentials credentials;
  if(controller.takeCredentials(credentials)) controller.unlockResult(now,unlockAuthorization(credentials));
  credentials.clear();
  if(controller.view().screen==Screen::unlock || controller.view().screen==Screen::settings ||
     controller.view().screen==Screen::administration || controller.view().screen==Screen::system) setCaptureProtected(true);
  Command command;
  if(controller.take(command)) controller.result(now,dispatch(command));
  FormInput input;
  if(controller.view().screen==Screen::settings) {
    if(!settingsActive) { settingsForm.open(settingsNow);settingsActive=true;presentedSettingsValid=false;attempted=false; }
    settingsForm.tick(settingsNow);
    if(controller.takeFormInput(input)) {
      if(input.activate && !(hasPresented && presentedSettingsValid &&
          sameSettings(settingsForm.view(),presentedSettings)))input.activate=false;
      if(settingsForm.closed()) {if(input.turn || input.activate || input.back)controller.closeForm();}
      else {
        if(input.back) settingsForm.back(settingsNow);
        else {
          if(input.turn) settingsForm.turn(input.turn,settingsNow);
          if(input.activate && sameSettings(settingsForm.view(),presentedSettings)) settingsForm.activate(settingsNow);
        }
      }
    }
    if(settingsForm.closed() && settingsForm.view().notice==Settings::Notice::none) controller.closeForm();
  }
  if(controller.view().screen!=Screen::settings && settingsActive) {
    settingsForm.discard();settingsActive=false;presentedSettingsValid=false;
  }
  if(controller.view().screen==Screen::administration) {
    if(!administrationActive) {administration.open(settingsNow);administrationActive=true;presentedAdministrationValid=false;attempted=false;}
    administration.tick(settingsNow);
    if(controller.takeFormInput(input)) {
      if(administration.closed()) {
        if(input.back || (input.activate && presentedAdministrationValid &&
            sameAdministration(administration.view(),presentedAdministration)))controller.closeForm();
      } else if(input.back)administration.back(settingsNow);
      else {
        if(input.turn)administration.turn(input.turn,settingsNow);
        if(input.activate && presentedAdministrationValid && presentedAdministrationEpoch==administration.epoch() &&
           sameAdministration(administration.view(),presentedAdministration))administration.activate(settingsNow);
      }
    }
    if(administration.takePrivateProjectionDiscard())discardPrivateProjection();
    if(administration.closed() && administration.view().notice==EntityAdmin::Notice::none)controller.closeForm();
  }
  if(controller.view().screen!=Screen::administration && administrationActive) {
    administration.discard();
    if(administration.takePrivateProjectionDiscard())discardPrivateProjection();
    administrationActive=presentedAdministrationValid=false;
  }
  if(controller.view().screen==Screen::system) {
    if(!systemActive) {systemForm.open(settingsNow);systemActive=true;presentedAdministrationValid=false;attempted=false;}
    systemForm.tick(settingsNow);
    auto renderCurrent=[&](){renderSystem(systemView);};
    visitSystemChild([&](auto &form){form.tick(settingsNow);});
    renderCurrent();
    if(controller.takeFormInput(input)) {
      if(systemChild!=SystemChild::none) {
        systemForm.childActivity(settingsNow);
        if(childClosed()) {
          if(input.back || (input.activate && presentedAdministrationValid && sameAdministration(systemView,presentedAdministration))) {
            discardSystemChild();systemForm.finishChild();
          }
        } else if(input.back)visitSystemChild([&](auto &form){form.back(settingsNow);});
        else {
          if(input.turn)visitSystemChild([&](auto &form){form.turn(input.turn,settingsNow);});
          renderCurrent();
          if(input.activate && presentedAdministrationValid && sameAdministration(systemView,presentedAdministration))
            visitSystemChild([&](auto &form){form.activate(settingsNow);});
        }
      } else if(systemForm.closed()) {
        if(input.back || (input.activate && presentedAdministrationValid && sameAdministration(systemView,presentedAdministration)))controller.closeForm();
      } else if(input.back)systemForm.back(settingsNow);
      else {
        if(input.turn)systemForm.turn(input.turn,settingsNow);
        systemForm.render(systemView);
        if(input.activate && presentedAdministrationValid && sameAdministration(systemView,presentedAdministration))systemForm.activate(settingsNow);
      }
    }
    if(systemChild==SystemChild::none && (systemForm.wantsWifiScan() || systemForm.wantsDiagnostics() || systemForm.wantsRadioScan())) {
      systemChild=systemForm.wantsWifiScan()?SystemChild::wifi:systemForm.wantsDiagnostics()?SystemChild::diagnostics:SystemChild::radio;
      visitSystemChild([&](auto &form){form.open(settingsNow);});
      presentedAdministrationValid=false;attempted=false;
    }
    if(systemChild==SystemChild::wifi) {
      char ssid[33];SecuritySnapshot expected;uint64_t networkRevision=0;
      if(wifiScanForm.takeSSID(ssid,settingsNow,expected,networkRevision)) {
        // Release/cancel observation storage BEFORE admitting another draft.
        discardSystemChild();systemForm.finishChild();
        Settings::Authorization auth{expected.allowed,expected.generation,expected.configurationGeneration};
        settingsForm.openNetworkSSID(ssid[0]?ssid:nullptr,settingsNow,auth,networkRevision);
        for(auto &byte:ssid)static_cast<volatile char&>(byte)=0;
        settingsActive=true;presentedSettingsValid=false;attempted=false;controller.openSettingsFromSystem();
      }
    }
    if(systemChild==SystemChild::radio){
      float frequency=0;SecuritySnapshot expected;uint64_t radioRevision=0;
      if(radioScanForm.takeFrequency(frequency,expected,radioRevision,settingsNow)){
        discardSystemChild();systemForm.finishChild();
        Settings::Authorization auth{expected.allowed,expected.generation,expected.configurationGeneration};
        settingsForm.openRadioFrequency(frequency,settingsNow,auth,radioRevision);
        settingsActive=true;presentedSettingsValid=false;attempted=false;controller.openSettingsFromSystem();
      }
    }
    renderCurrent();
    if(systemChild!=SystemChild::none && childClosed() && systemView.notice==EntityAdmin::Notice::none) {
      discardSystemChild();systemForm.finishChild();renderCurrent();
    }
    if(systemForm.closed()) {
      discardSystemChild();
      if(systemView.notice==EntityAdmin::Notice::none)controller.closeForm();
    }
  }
  if(controller.view().screen!=Screen::system && systemActive) {
    discardSystemChild();wipePrivateProjection(systemView);systemForm.discard();systemActive=false;presentedAdministrationValid=false;
  }
  if(!controller.brightness() && !wasSleeping) lockAuthorization();
  wasSleeping=!controller.brightness();light(controller.brightness());
  if(!controller.brightness()) {capturePoll(memory.pixels,bandIndex==43,0,revision);return true;}
  const View &view=controller.view();
  if(view.screen==Screen::catalogue && (view.catalogue!=model.catalogue || view.room!=model.room || view.cursor!=model.page.cursor)) return true;
  if(view.screen==Screen::control && view.notice!=Notice::changed &&
     (view.entity!=model.target.ref || (!view.openedToken && model.target.token))) return true;
  const uint64_t administrationEpoch=view.screen==Screen::administration?administration.epoch():0;
  const uint32_t desired=view.screen==Screen::settings?settingsHash(settingsForm.view()):
    view.screen==Screen::administration?administrationHash(administration.view(),administrationEpoch):
    view.screen==Screen::system?administrationHash(systemView,0):sceneHash(model,view,s);
  const bool meaning=sameMeaning(view,frameView,model.pageToken,framePageToken,model.target.token,frameTargetToken) &&
    (view.screen!=Screen::settings || sameSettings(settingsForm.view(),frameSettings)) &&
    (view.screen!=Screen::administration || (sameAdministration(administration.view(),frameAdministration) && administrationEpoch==frameAdministrationEpoch)) &&
    (view.screen!=Screen::system || sameAdministration(systemView,frameAdministration));
  if((desired!=scene || !meaning || !attempted) && (bandIndex==43 || !meaning || !attempted)) {
    const uint32_t start=micros();
    if(view.screen==Screen::settings) canvas.paintSettings(settingsForm.view());
    else if(view.screen==Screen::administration)canvas.paintEntity(administration.view());
    else if(view.screen==Screen::system)canvas.paintEntity(systemView);
    else canvas.paint(model,view,s);
    const uint32_t elapsed=micros()-start;
    if(elapsed>maxPaintUs) maxPaintUs=elapsed;
    if(view.screen==Screen::settings) frameSettings=settingsForm.view();
    if(view.screen==Screen::administration) {frameAdministration=administration.view();frameAdministrationEpoch=administrationEpoch;}
    if(view.screen==Screen::system) {frameAdministration=systemView;frameAdministrationEpoch=0;}
    frameView=view;frameTargetToken=model.target.ref==view.entity?model.target.token:0;
    framePageToken=model.pageToken;scene=desired;bandIndex=0;attempted=true;++revision;
  }
  capturePoll(memory.pixels,bandIndex==43,controller.brightness(),revision);
  if(bandIndex==43 || uint32_t(now-lastBand)<2) return true;
  lastBand=now;
  const uint8_t y=bandIndex*bandHeight, rows=(screenHeight-y)<bandHeight?screenHeight-y:bandHeight;
  const size_t count=screenWidth*rows,bytes=count*sizeof(uint16_t);
  const auto *pixels=memory.pixels+y*screenWidth;const uint32_t content=hash(pixels,bytes);
  if(!(validBands&(uint64_t(1)<<bandIndex)) || bandHashes[bandIndex]!=content) {
    for(size_t i=0;i<count;i++) transfer[i]=(pixels[i]<<8)|(pixels[i]>>8);
    if(!sendBand(y,rows,reinterpret_cast<uint8_t *>(transfer),bytes)) return true;
    bandHashes[bandIndex]=content;validBands|=uint64_t(1)<<bandIndex;
  }
  if(++bandIndex==43) {
    if(frameView.screen==Screen::settings) {presentedSettings=frameSettings;presentedSettingsValid=true;}
    else presentedSettingsValid=false;
    if(frameView.screen==Screen::administration || frameView.screen==Screen::system) {presentedAdministration=frameAdministration;presentedAdministrationEpoch=frameAdministrationEpoch;presentedAdministrationValid=true;}
    else presentedAdministrationValid=false;
    presentedView=frameView;presentedTargetToken=frameTargetToken;presentedPageToken=framePageToken;hasPresented=true;
    if(frameView.screen==Screen::control && frameView.openedToken && frameView.openedToken==frameTargetToken) {
      stopPresentedRef=frameView.entity;stopPresentedToken=frameTargetToken;
      markControlPresented(frameView.entity,frameTargetToken,frameView.authorizedSelection,frameView.selectionAuthGeneration);
    }
    if(frameView.screen!=Screen::unlock && frameView.screen!=Screen::settings && frameView.screen!=Screen::administration && frameView.screen!=Screen::system) setCaptureProtected(false);
  }
  return true;
}
} // namespace LocalUi
#endif

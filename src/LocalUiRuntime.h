#pragma once
#ifdef ESPSOMFY_LOCAL_UI
#include "LocalUi.h"
#include "LocalUiMemory.h"
#include "LocalUiRenderer.h"
#include "LocalSettingsRuntime.h"
#include "LocalAdministration.h"
#include "LocalSystem.h"
#include "LocalWifiScan.h"
#include "LocalDiagnostics.h"
#include "LocalUpdates.h"
#include "LocalFrequencyScan.h"
#include <type_traits>
#include <new>
namespace LocalUi {
constexpr size_t largerSystemChild(size_t a,size_t b){return a>b?a:b;}
constexpr size_t systemChildBytes=largerSystemChild(largerSystemChild(sizeof(WifiScan::Form),sizeof(Diagnostics::Form)),largerSystemChild(sizeof(RadioScan::Form),sizeof(Updates::Form)));
static_assert(sizeof(System::Form)+systemChildBytes+2<=160,"System children share one bounded handle and existing presentation storage");
class Runtime {
#ifdef UI_RUNTIME_HOST_TEST
  friend struct RuntimeTestAccess;
#endif
  GraphicsMemory memory;
  Canvas canvas;
  Controller controller;
  Encoder encoder;
  Settings::Form settingsForm{Settings::runtimeBackend(),Settings::runtimeMemory()};
  Settings::FormView frameSettings, presentedSettings;
  Administration administration;
  System::Form systemForm{System::runtimeBackend()};
  // Exactly one child is alive. Its destructor cancels observers/owned work
  // before another handle can reuse this storage; sessions keep the wiping pool.
  union {
    WifiScan::Form wifiScanForm;
    Diagnostics::Form diagnosticsForm;
    RadioScan::Form radioScanForm;
    Updates::Form updatesForm;
  };
  enum class SystemChild:uint8_t {none,wifi,diagnostics,radio,updates} systemChild=SystemChild::none;
  bool systemActive=false;
  template<typename Action> void visitSystemChild(Action action) {
    if(systemChild==SystemChild::wifi)action(wifiScanForm);
    else if(systemChild==SystemChild::diagnostics)action(diagnosticsForm);
    else if(systemChild==SystemChild::radio)action(radioScanForm);
    else if(systemChild==SystemChild::updates)action(updatesForm);
  }
  static void wipePrivateProjection(EntityAdmin::FormView &view) {
    static_assert(std::is_trivially_copyable<EntityAdmin::FormView>::value,
                  "Only plain projected fields may be byte-erased");
    static_assert(sizeof(EntityAdmin::FormView)<=352,"Bound private projection erasure");
    auto *bytes=reinterpret_cast<volatile unsigned char *>(&view);
    for(size_t i=0;i<sizeof(view);++i)bytes[i]=0;
    view.protectedFrame=true;
  }
  void discardPrivateProjection(){
    // Retained current/presented projections can outlive a blocked SPI frame.
    // Erase both after private data teardown; accept only a fresh full frame.
    wipePrivateProjection(frameAdministration);wipePrivateProjection(presentedAdministration);
    frameAdministrationEpoch=presentedAdministrationEpoch=0;presentedAdministrationValid=false;
  }
  void discardSystemChild(){
    visitSystemChild([](auto &form){using Child=typename std::remove_reference<decltype(form)>::type;form.~Child();});
    systemChild=SystemChild::none;
    discardPrivateProjection();
  }
  void beginSystemChild(SystemChild next){
    discardSystemChild();
    switch(next){
      case SystemChild::wifi:new(&wifiScanForm)WifiScan::Form(WifiScan::runtimeBackend(),Settings::runtimeMemory());break;
      case SystemChild::diagnostics:new(&diagnosticsForm)Diagnostics::Form(Diagnostics::runtimeBackend(),Settings::runtimeMemory());break;
      case SystemChild::radio:new(&radioScanForm)RadioScan::Form(RadioScan::runtimeBackend(),Settings::runtimeMemory());break;
      case SystemChild::updates:new(&updatesForm)Updates::Form(Updates::runtimeBackend(),Settings::runtimeMemory());break;
      case SystemChild::none:break;
    }
    systemChild=next;
  }
  bool childClosed()const {
    return systemChild==SystemChild::wifi?wifiScanForm.closed():systemChild==SystemChild::diagnostics?diagnosticsForm.closed():systemChild==SystemChild::radio?radioScanForm.closed():systemChild==SystemChild::updates?updatesForm.closed():false;
  }
  void renderSystem(EntityAdmin::FormView &out)const {
    if(systemChild==SystemChild::wifi)wifiScanForm.render(out);
    else if(systemChild==SystemChild::diagnostics)diagnosticsForm.render(out);
    else if(systemChild==SystemChild::radio)radioScanForm.render(out);
    else if(systemChild==SystemChild::updates)updatesForm.render(out);
    else systemForm.render(out);
  }
  EntityAdmin::FormView frameAdministration,presentedAdministration;
  uint64_t frameAdministrationEpoch=0,presentedAdministrationEpoch=0;
  bool administrationActive=false,presentedAdministrationValid=false;
  bool settingsActive=false, presentedSettingsValid=false;
  uint32_t bandHashes[43] = {};
  uint64_t validBands = 0;
  uint32_t scene = 0, lastBand = 0, revision = 0;
  uint8_t bandIndex = 43;
  bool attempted = false, hasPresented = false, wasSleeping = false, graphicsMissing = false;
  View frameView, presentedView;
  uint64_t frameTargetToken=0,presentedTargetToken=0,framePageToken=0,presentedPageToken=0,stopPresentedToken=0;
  EntityRef stopPresentedRef;

public:
  Runtime();
  ~Runtime(){discardSystemChild();}
  uint32_t maxPaintUs = 0;
  void begin();
  DisplayPreferences preferences() const { return controller.preferences(); }
  GraphicsFailure failure() const { return memory.failure; }
  bool healthy() const {return memory.failure==GraphicsFailure::none && hasPresented && !graphicsMissing;}
  bool tick(uint32_t now, const StatusDisplay::Snapshot &, bool pressed,
            bool (*sendBand)(uint8_t, uint8_t, const uint8_t *, size_t), void (*light)(uint8_t));
};
} // namespace LocalUi
#endif

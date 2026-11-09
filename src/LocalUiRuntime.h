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
namespace LocalUi {
static_assert(sizeof(System::Form)+sizeof(WifiScan::Form)+sizeof(Diagnostics::Form)+sizeof(RadioScan::Form)+sizeof(Updates::Form)+2<=160,"System observation state borrows existing presentation storage");
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
  WifiScan::Form wifiScanForm{WifiScan::runtimeBackend(),Settings::runtimeMemory()};
  Diagnostics::Form diagnosticsForm{Diagnostics::runtimeBackend(),Settings::runtimeMemory()};
  Updates::Form updatesForm{Updates::runtimeBackend(),Settings::runtimeMemory()};
  RadioScan::Form radioScanForm{RadioScan::runtimeBackend(),Settings::runtimeMemory()};
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
    wifiScanForm.discard();diagnosticsForm.discard();radioScanForm.discard();updatesForm.discard();systemChild=SystemChild::none;
    discardPrivateProjection();
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

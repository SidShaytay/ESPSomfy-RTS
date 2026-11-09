#pragma once
#ifdef ESPSOMFY_LOCAL_UI
#include "LocalUi.h"
#include "LocalUiMemory.h"
#include "LocalUiRenderer.h"
#include "LocalSettingsRuntime.h"
#include "LocalAdministration.h"
#include "LocalSystem.h"
namespace LocalUi {
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
  bool systemActive=false;
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
  bool tick(uint32_t now, const StatusDisplay::Snapshot &, bool pressed,
            bool (*sendBand)(uint8_t, uint8_t, const uint8_t *, size_t), void (*light)(uint8_t));
};
} // namespace LocalUi
#endif

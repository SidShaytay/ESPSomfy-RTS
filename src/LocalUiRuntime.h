#pragma once
#ifdef ESPSOMFY_LOCAL_UI
#include "LocalUi.h"
#include "LocalUiMemory.h"
#include "LocalUiRenderer.h"
namespace LocalUi {
class Runtime {
  GraphicsMemory memory;
  Canvas canvas;
  Controller controller;
  Encoder encoder;
  uint32_t bandHashes[43] = {};
  uint64_t validBands = 0;
  uint32_t scene = 0, lastBand = 0, revision = 0;
  uint8_t bandIndex = 43;
  bool attempted = false, hasPresented = false;
  View frameView, presentedView;
  uint32_t frameIdentity = 0, presentedIdentity = 0;

public:
  uint32_t maxPaintUs = 0;
  void begin();
  uint8_t brightnessPreference() const { return controller.preference(); }
  GraphicsFailure failure() const { return memory.failure; }
  bool tick(uint32_t now, const StatusDisplay::Snapshot &, bool pressed,
            bool (*sendBand)(uint8_t, uint8_t, const uint8_t *, size_t), void (*light)(uint8_t));
};
} // namespace LocalUi
#endif

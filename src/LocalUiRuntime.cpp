#ifdef ESPSOMFY_LOCAL_UI
#include "LocalUiRuntime.h"
#include "LocalUiAdapter.h"
#include "LocalUiCapture.h"
#include "LocalUiAuthorizationAdapter.h"
#include "BoardProfile.h"
#include <esp_heap_caps.h>
#include <string.h>
namespace LocalUi {
namespace {
uint32_t hash(const void *p, size_t n, uint32_t h = 2166136261u) {
  const auto *b = static_cast<const uint8_t *>(p);
  while(n--)
    h = (h ^ *b++) * 16777619u;
  return h;
}
uint32_t sceneHash(const Model &m, const View &v, const StatusDisplay::Snapshot &s) {
  uint32_t h = 2166136261u;
  const uint32_t values[] = {uint32_t(v.screen),
                             v.selected,
                             uint32_t(v.action),
                             uint32_t(v.notice),
                             v.level,
                             v.setting,
                             v.detailsOffset,
                             v.dimSeconds,
                             v.offSeconds,
                             uint32_t(s.battery.state),
                             s.battery.percent,
                             s.battery.millivolts,
                             v.credentialChoice, v.credentialLength, v.credentialMaximum,
                             v.credentialUser, v.credentialByte, v.retryAfterMs/1000,
                             m.authorized, m.authGeneration, uint32_t(m.authMode),
                             m.ready,
                             m.controlsEnabled,
                             s.running,
                             uint32_t(s.link),
                             uint32_t(s.web),
                             uint32_t(s.radio),
                             uint32_t(s.frequency * 100),
                             uint32_t(s.uptimeSeconds / 60)};
  h = hash(values, sizeof(values), h);
  h = hash(s.address, strlen(s.address), h);
  h = hash(s.build, strlen(s.build), h);
  h = hash(s.firmwareVersion, strlen(s.firmwareVersion), h);
  h = hash(s.applicationVersion, strlen(s.applicationVersion), h);
  for(const auto &t : m.targets) {
    const uint32_t x[] = {t.identity, t.members[0],        t.members[1],       t.valid,
                          t.moving, t.stopLease, uint32_t(t.open[0]), uint32_t(t.open[1])};
    h = hash(x, sizeof(x), h);
    h = hash(t.name, strlen(t.name), h);
  }
  return h;
}
void *allocate(size_t size) { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
// Fixed internal bounce buffer, not a second framebuffer and not DMA-owned.
uint16_t transfer[screenWidth * bandHeight];
} // namespace
void Runtime::begin() {
  setCaptureProtected(true);
  controller.setPreferences(loadPreferences());
  pinMode(BoardProfile::backButton, INPUT_PULLUP);
  memory.begin(heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0, allocate);
  if(memory.pixels) {
    canvas.attach(memory.pixels);
    pinMode(BoardProfile::encoderA, INPUT_PULLUP);
    pinMode(BoardProfile::encoderB, INPUT_PULLUP);
  }
}
bool Runtime::tick(uint32_t now, const StatusDisplay::Snapshot &s, bool pressed,
                   bool (*sendBand)(uint8_t, uint8_t, const uint8_t *, size_t), void (*light)(uint8_t)) {
  if(!memory.pixels) {
    setCaptureProtected(false);
    capturePoll(nullptr, true, 0, 0);
    return false;
  }
  const View &before=controller.view();
  focusAuthorizationTarget(before.screen==Screen::control || before.screen==Screen::unlock ? before.selected : 255);
  const Model model = snapshotModel(s.running);
  const View &focus = controller.view();
  const uint32_t identity =
      focus.screen == Screen::control && focus.selected < 3 ? model.targets[focus.selected].identity : 0;
  // Position/uptime repainting must not disable an already visible STOP button.
  // Only a change of page, target, identity or selected action needs re-presentation.
  const bool visible = hasPresented && presentedView.screen == focus.screen &&
                       presentedView.selected == focus.selected && presentedView.action == focus.action &&
                       presentedIdentity == identity;
  const int8_t turn =
      encoder.sample((digitalRead(BoardProfile::encoderA) << 1) | digitalRead(BoardProfile::encoderB));
  controller.tick(now, model, turn, pressed, visible, digitalRead(BoardProfile::backButton) == 0);
  DisplayPreferences preferences;
  if(controller.takePreferences(preferences))
    controller.brightnessResult(now, model.authorized && savePreferences(preferences));
  if(controller.takeActivity()) authorizationActivity();
  Credentials credentials;
  if(controller.takeCredentials(credentials))
    controller.unlockResult(now, unlockAuthorization(credentials));
  credentials.clear();
  if(controller.view().screen == Screen::unlock) setCaptureProtected(true);
  Command command;
  if(controller.take(command))
    controller.result(now, dispatch(command));
  if(!controller.brightness() && !wasSleeping) lockAuthorization();
  wasSleeping=!controller.brightness();
  light(controller.brightness());
  if(!controller.brightness()) {
    capturePoll(memory.pixels, bandIndex == 43, 0, revision);
    return true;
  }
  const uint32_t desired = sceneHash(model, controller.view(), s);
  if(desired != scene || !attempted) {
    const uint32_t start = micros();
    canvas.paint(model, controller.view(), s);
    const uint32_t elapsed = micros() - start;
    if(elapsed > maxPaintUs)
      maxPaintUs = elapsed;
    frameView = controller.view();
    frameIdentity = frameView.screen == Screen::control && frameView.selected < 3
                        ? model.targets[frameView.selected].identity
                        : 0;
    scene = desired;
    bandIndex = 0;
    attempted = true;
    ++revision;
  }
  capturePoll(memory.pixels, bandIndex == 43, controller.brightness(), revision);
  if(bandIndex == 43 || uint32_t(now - lastBand) < 2)
    return true;
  lastBand = now;
  const uint8_t y = bandIndex * bandHeight,
                rows = (screenHeight - y) < bandHeight ? screenHeight - y : bandHeight;
  const size_t count = screenWidth * rows, bytes = count * sizeof(uint16_t);
  const auto *pixels = memory.pixels + y * screenWidth;
  const uint32_t content = hash(pixels, bytes);
  if(!(validBands & (uint64_t(1) << bandIndex)) || bandHashes[bandIndex] != content) {
    // Copy/byte-swap outside the radio mutex. Only the bounded SPI write holds it.
    for(size_t i = 0; i < count; i++)
      transfer[i] = (pixels[i] << 8) | (pixels[i] >> 8);
    if(!sendBand(y, rows, reinterpret_cast<uint8_t *>(transfer), bytes))
      return true;
    bandHashes[bandIndex] = content;
    validBands |= uint64_t(1) << bandIndex;
  }
  ++bandIndex;
  if(bandIndex == 43) {
    presentedView = frameView;
    presentedIdentity = frameIdentity;
    hasPresented = true;
    if(presentedView.screen != Screen::unlock) setCaptureProtected(false);
  }
  return true;
}
} // namespace LocalUi
#endif

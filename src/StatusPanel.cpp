#include "StatusPanel.h"
#ifdef ESPSOMFY_STATUS_DISPLAY
#if !defined(ESPSOMFY_LILYGO_T_EMBED_CC1101)
#error "Status LCD requires an exact supported LILYGO board profile"
#endif
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include "BoardProfile.h"
#include "DisplayBus.h"
#include "StatusDisplay.h"
#include "GitUpdate.h"
#include "GitUpdateMemory.h"
#ifdef ESPSOMFY_LOCAL_UI
#include "LocalUiRuntime.h"
#include "LocalUiCapture.h"
#endif

namespace StatusPanel {
using namespace StatusDisplay;
namespace {
constexpr uint8_t dc = BoardProfile::displayDC, wake = BoardProfile::wakeButton;
// No framebuffer/PSRAM or DMA allocation: one monochrome text row (720 bytes)
// plus a 640-byte internal transfer line. Never let a graphics driver own SPI.
class Panel : public Output {
  GFXcanvas1 canvas{320, 18};
  uint16_t pixels[320] = {};
  uint8_t phase = 0, clearY = 0, light = 0;
  uint32_t next = 0, started = 0;
  bool began = false, busFailed = false, requireText = true;
  void command(uint8_t value, const uint8_t *data = nullptr, size_t size = 0) {
    digitalWrite(dc, LOW); SPI.transfer(value);
    digitalWrite(dc, HIGH);
    if(size) SPI.writeBytes(data, size);
  }
  void window(uint8_t y, uint8_t height) {
    const uint8_t x[] = {0, 0, 1, 63};
    const uint8_t rows[] = {0, uint8_t(y + 35), 0, uint8_t(y + height + 34)};
    command(0x2a, x, sizeof(x)); command(0x2b, rows, sizeof(rows)); command(0x2c);
  }
  template<class F> bool transfer(F fn) {
    if(busFailed) return false;
    struct Operation { F *call; uint32_t *maxUs; bool *failed; } operation{&fn, &maxBusUs, &busFailed};
    const bool acquired = tryDisplayBus([](void *context) {
      auto &op = *static_cast<Operation *>(context);
      const uint32_t start = micros();
      // The radio driver may have ended SPI. Reuse its exact bus/pins, under
      // its mutex. Never end/remap the bus or change SCK/MOSI/MISO pin modes.
      if(!SPI.begin(BoardProfile::sck, BoardProfile::miso, BoardProfile::mosi, BoardProfile::cs)) {
        *op.failed = true;
        return;
      }
      SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
      digitalWrite(BoardProfile::displayCS, LOW);
      (*op.call)();
      digitalWrite(BoardProfile::displayCS, HIGH);
      SPI.endTransaction();
      const uint32_t elapsed = micros() - start;
      if(elapsed > *op.maxUs) *op.maxUs = elapsed;
    }, &operation);
    return acquired && !busFailed;
  }
public:
  uint32_t maxRowUs = 0, maxBusUs = 0, skipped = 0;
  void requireFallbackText(bool required) {requireText=required;}
  bool healthy() const {return !busFailed && phase==20 && clearY==170;}
  Result initialize(uint32_t now) override {
    if(busFailed) return Result::failed;
    if(phase==20 && clearY==170) return Result::ready;
    if(!began) {
      began = true; started = now;
      pinMode(dc, OUTPUT); pinMode(wake, INPUT_PULLUP);
      if((requireText && !canvas.getBuffer()) || !ledcAttach(BoardProfile::backlight, 5000, 8)) return Result::failed;
      ledcWrite(BoardProfile::backlight, 0);
    }
    if(uint32_t(now - started) > 5000) return Result::failed;
    if(int32_t(now - next) < 0) return Result::pending;
    // ST7789V 170x320, manufacturer setup 214; landscape MADCTL=MX|MV,
    // row offset 35. Reset is software only (manufacturer TFT_RST=-1).
    // Separate reset/sleep delays from bus ownership, never delay with the lock.
    static const uint8_t init[][16] = {
      {0x01,0}, {0x11,0}, {0x13,0}, {0x36,1,0x60}, {0xb6,2,0x0a,0x82},
      {0xb0,2,0x00,0xe0}, {0x3a,1,0x55}, {0xb2,5,0x0c,0x0c,0,0x33,0x33},
      {0xb7,1,0x35}, {0xbb,1,0x28}, {0xc0,1,0x0c}, {0xc2,2,1,0xff},
      {0xc3,1,0x10}, {0xc4,1,0x20}, {0xc6,1,0x0f}, {0xd0,2,0xa4,0xa1},
      {0xe0,14,0xd0,0,2,7,0x0a,0x28,0x32,0x44,0x42,6,0x0e,0x12,0x14,0x17},
      {0xe1,14,0xd0,0,2,7,0x0a,0x28,0x31,0x54,0x47,0x0e,0x1c,0x17,0x1b,0x1e},
      {0x21,0}, {0x29,0}
    };
    if(phase < sizeof(init)/sizeof(init[0])) {
      if(!transfer([&] { command(init[phase][0], &init[phase][2], init[phase][1]); }))
        return busFailed ? Result::failed : Result::pending;
      next = now + ((phase == 0 || phase == 1 || phase == 19) ? 120 : 10);
      ++phase;
      return Result::pending;
    }
    if(clearY < 170) {
      // Clear two scanlines per tick: at most 1280 bytes under the lock.
      if(!transfer([&] { window(clearY, 2); SPI.writeBytes(reinterpret_cast<uint8_t *>(pixels), sizeof(pixels));
          SPI.writeBytes(reinterpret_cast<uint8_t *>(pixels), sizeof(pixels)); }))
        return busFailed ? Result::failed : Result::pending;
      clearY += 2;
      return Result::pending;
    }
    return Result::ready; // write-only panel: command completion, not detection
  }
  bool row(uint8_t index, const char *text) override {
    if(index >= 9 || busFailed) return false;
    const uint32_t start = micros();
    canvas.fillScreen(0); canvas.setCursor(2, 1); canvas.setTextColor(1);
    canvas.setTextSize(2); canvas.setTextWrap(false); canvas.print(text);
    const bool done = transfer([&] {
      window(index * 18, 18);
      for(uint8_t y = 0; y < 18; ++y) {
        for(uint16_t x = 0; x < 320; ++x) pixels[x] = canvas.getPixel(x, y) ? 0xffff : 0;
        SPI.writeBytes(reinterpret_cast<uint8_t *>(pixels), sizeof(pixels));
      }
    });
    if(!done) ++skipped;
    else if(micros() - start > maxRowUs) maxRowUs = micros() - start;
    return done;
  }
  bool band(uint8_t y,uint8_t rows,const uint8_t *data,size_t bytes) {
    if(!data || !rows || rows>4 || y+rows>170 || bytes!=size_t(rows)*320*2 || busFailed)return false;
    const uint32_t start=micros();
    const bool done=transfer([&] {window(y,rows);SPI.writeBytes(data,bytes);});
    if(!done)++skipped;
    else if(micros()-start>maxRowUs)maxRowUs=micros()-start;
    return done;
  }
  void brightness(uint8_t value) override {
    if(busFailed) value = 0;
    if(value != light) { light = value; ledcWrite(BoardProfile::backlight, value); }
  }
};
Panel panel;
#ifdef ESPSOMFY_LOCAL_UI
Session session(panel,20000);
#else
Session session(panel);
#endif
#ifdef ESPSOMFY_LOCAL_UI
LocalUi::Runtime ui;
static bool sendBand(uint8_t y,uint8_t rows,const uint8_t *data,size_t bytes) {return panel.band(y,rows,data,bytes);}
static void light(uint8_t value) {panel.brightness(value);}
#endif
}
// Implemented in the adapter: this driver has no command/model dependency.
Snapshot snapshot(bool running);
void pollPower(bool running);
void begin() {
#ifdef ESPSOMFY_LOCAL_UI
  ui.begin();
  panel.requireFallbackText(ui.failure()!=LocalUi::GraphicsFailure::none);
  const auto prefs=ui.preferences();
  session.setBrightness(prefs.brightness);session.setTimeouts(uint32_t(prefs.dimSeconds)*1000,uint32_t(prefs.offSeconds)*1000);
#endif
  // Show BOOT while the rest of setup has not yet run. No radio task exists.
  const uint32_t start = millis();
  while(millis() - start < 1200) { loop(false); delay(1); }
}
void loop(bool running) {
  pollPower(running);
  static Snapshot s;
  static uint32_t sampled = 0;
  if(!running || !s.running || millis() - sampled >= 1000) {
    s = snapshot(running);
    sampled = millis();
  }
#ifdef ESPSOMFY_LOCAL_UI
  const auto initialized=panel.initialize(millis());
  if(initialized==Result::failed) {panel.brightness(0);LocalUi::capturePoll(nullptr,true,0,0);return;}
  if(initialized==Result::pending)return;
  s.recovery=gitUpdateRecoveryNotice(); // Decision changes need not wait for the one-second telemetry sample.
  s.graphicsFailure=uint8_t(ui.failure());
  if(!ui.tick(millis(),s,digitalRead(wake)==LOW,sendBand,light))
#endif
    session.tick(millis(), s, digitalRead(wake) == LOW
#ifdef ESPSOMFY_LOCAL_UI
      || digitalRead(BoardProfile::backButton)==LOW
#endif
    );
  // Private diagnostics deliberately contain only memory/timing, never model data.
  static uint32_t logged = 0;
  if(millis() - logged >= 30000) {
    logged = millis();
#ifdef ESPSOMFY_LOCAL_UI
    Serial.printf("LCD UI PSRAM=%u paintUs=%lu fallback=%u\n", unsigned(ui.failure()==LocalUi::GraphicsFailure::none?LocalUi::graphicsBytes:0), (unsigned long)ui.maxPaintUs, unsigned(ui.failure()));
#endif
    // Keep the full updater snapshot transient; never print its identity fields.
    // Both accessors release their own locks before the first Serial call below.
    struct UpdateMetrics {size_t minInternal, minLargest, maxPsram, workerStack;GitUpdateTransport transport;};
    static_assert(sizeof(GitUpdateSnapshot)<=512, "Bound diagnostic snapshot stack");
    const auto update=[] {
      const auto sampled=gitUpdate.snapshot();
      return UpdateMetrics{sampled.minInternalFree,sampled.minLargestInternal,
        sampled.maxPsramUsed,sampled.workerStackMinimum,sampled.transport};
    }();
    const auto tls=gitUpdateMemoryStats();
    // Heap/stack extrema are sampled; workerStack=0 is unsampled.
    // TLS requested counts include allocator bookkeeping, not SDK heap overhead.
    Serial.printf("LCD update minInternalB=%lu minLargestInternalB=%lu maxPsramUsedB=%lu workerStackMinB=%lu tlsRequestedB=%lu tlsPeakRequestedB=%lu tlsInternalPeakRequestedB=%lu tlsFailures=%lu download=%u stage=%u sdkErr=%ld http=%ld errno=%ld tlsSdkErr=%ld tlsErr=%ld tlsFlags=%ld certState=%u certFailure=%u\n",
      (unsigned long)update.minInternal,(unsigned long)update.minLargest,
      (unsigned long)update.maxPsram,(unsigned long)update.workerStack,
      (unsigned long)tls.requested,(unsigned long)tls.peak,
      (unsigned long)tls.internalPeak,(unsigned long)tls.failures,
      unsigned(update.transport.download),unsigned(update.transport.stage),
      (long)update.transport.sdkError,(long)update.transport.httpStatus,(long)update.transport.socketError,
      (long)update.transport.tlsSdkError,(long)update.transport.tlsError,(long)update.transport.tlsFlags,
      unsigned(update.transport.certificateState),unsigned(update.transport.certificateFailure));
    Serial.printf("LCD battery state=%u percent=%u mV=%u mA=%d\n", unsigned(s.battery.state),
      unsigned(s.battery.percent), unsigned(s.battery.millivolts), int(s.battery.milliamps));
    Serial.printf("LCD internal=%u PSRAM=%u maxRowUs=%lu maxBusUs=%lu busySkips=%lu\n",
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
      (unsigned long)panel.maxRowUs, (unsigned long)panel.maxBusUs, (unsigned long)panel.skipped);
  }
}
bool healthy() {
#ifdef ESPSOMFY_LOCAL_UI
  return panel.healthy() && ui.healthy();
#else
  return panel.healthy();
#endif
}
}
#else
void StatusPanel::begin() {}
void StatusPanel::loop(bool) {}
bool StatusPanel::healthy(){return false;}
#endif

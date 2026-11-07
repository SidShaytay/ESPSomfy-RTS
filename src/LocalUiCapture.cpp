#ifdef ESPSOMFY_LOCAL_UI
#include "LocalUiCapture.h"
#include "LcdBitmap.h"
#include <ESPAsyncWebServer.h>
#include <esp_heap_caps.h>
#include <atomic>
#include <mutex>
#include <memory>
namespace LocalUi {
namespace {
std::mutex mutex;
AsyncWebServerRequestPtr waiting;
bool pending = false;
uint32_t requested = 0;
std::atomic<bool> busy{false};
} // namespace
void registerCapture(AsyncWebServer &server) {
  // Existing Web::requireToken middleware protects this exact non-public route.
  server.on(AsyncURIMatcher::exact("/lcd/screenshot"), HTTP_GET, [](AsyncWebServerRequest *r) {
    if(busy.exchange(true)) {
      r->send(429, "text/plain", "LCD capture already in progress");
      return;
    }
    std::lock_guard<std::mutex> lock(mutex);
    waiting = r->pause();
    requested = millis();
    pending = true;
  });
}
void capturePoll(const uint16_t *pixels, bool fullyPresented, uint8_t brightness, uint32_t revision) {
  AsyncWebServerRequestPtr weak;
  {
    std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
    if(!lock.owns_lock() || !pending)
      return;
    if(!waiting.expired() && pixels && !fullyPresented && uint32_t(millis() - requested) < 1500)
      return;
    weak = waiting;
    waiting.reset();
    pending = false;
  }
  auto request = weak.lock();
  if(!request) {
    busy = false;
    return;
  }
  if(!pixels || !fullyPresented) {
    request->send(503, "text/plain",
                  !pixels ? "No RGB framebuffer: simple status or disabled display"
                          : "LCD frame not yet fully transferred");
    busy = false;
    return;
  }
  auto *buffer =
      static_cast<uint8_t *>(heap_caps_malloc(LcdBitmap::totalBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if(!buffer) {
    request->send(503, "text/plain", "PSRAM screenshot allocation failed");
    busy = false;
    return;
  }
  // Immutable second PSRAM buffer only for the response lifetime. Copy occurs
  // on loop(), with no SPI/radio lock and no concurrent framebuffer writer.
  std::shared_ptr<uint8_t> image(buffer, [](uint8_t *p) {
    heap_caps_free(p);
    busy = false;
  });
  LcdBitmap::header(buffer);
  memcpy(buffer + LcdBitmap::headerBytes, pixels, LcdBitmap::pixelBytes);
  auto *response = request->beginResponse("image/bmp", LcdBitmap::totalBytes,
                                          [image](uint8_t *out, size_t maxLen, size_t index) -> size_t {
                                            if(index >= LcdBitmap::totalBytes)
                                              return 0;
                                            const size_t n = std::min(maxLen, LcdBitmap::totalBytes - index);
                                            memcpy(out, image.get() + index, n);
                                            return n;
                                          });
  if(!response) {
    request->send(503, "text/plain", "Screenshot response allocation failed");
    return;
  }
  response->addHeader("Cache-Control", "no-store");
  response->addHeader("Content-Disposition", "attachment; filename=\"lcd.bmp\"");
#ifdef ESPSOMFY_LOCAL_COMMANDS
  response->addHeader("X-LCD-Commands", "enabled");
#else
  response->addHeader("X-LCD-Commands", "locked");
#endif
  response->addHeader("X-LCD-Frame", String(revision));
  response->addHeader("X-LCD-Backlight", String(brightness));
  request->send(response);
}
} // namespace LocalUi
#endif

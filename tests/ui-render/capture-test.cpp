#include "LocalUiCapture.h"
#include "LcdBitmap.h"
#include <ESPAsyncWebServer.h>
#include <esp_heap_caps.h>
#include <vector>
#include <stdio.h>
int testPins[64] = {};
uint32_t testNow = 0;
bool hasPsram = true, allocateOK = true, failResponse = false;
void *allocated = nullptr;
int main() {
  AsyncWebServer server;
  LocalUi::registerCapture(server);
  std::vector<uint16_t> pixels(320 * 170, 0xf800);
  auto request = [&] {
    auto r = std::make_shared<AsyncWebServerRequest>();
    server.handler(r.get());
    return r;
  };
  LocalUi::setCaptureProtected(true);
  auto blocked=request();
  assert(blocked->status==423 && !allocated);
  assert(blocked->response->headers["Cache-Control"]=="no-store");
  failResponse=true;
  auto lowMemory=request();
  assert(lowMemory->status==503 && !allocated && !lowMemory->response);
  failResponse=false;
  LocalUi::setCaptureProtected(false);
  auto a = request();
  auto b = request();
  assert(b->status == 429);
  LocalUi::capturePoll(pixels.data(), false, 32, 7);
  assert(a->status == 0);
  a.reset();
  LocalUi::capturePoll(pixels.data(), false, 32, 7); // disconnected waiter is released
  a = request();
  LocalUi::setCaptureProtected(true); // waiter predates secret entry
  LocalUi::capturePoll(pixels.data(), true,32,7);
  assert(a->status==423 && !allocated);
  auto during=request(); assert(during->status==423 && !allocated);
  LocalUi::setCaptureProtected(false); // runtime may do this only after sanitized full presentation
  a=request();
  LocalUi::capturePoll(pixels.data(), true, 32, 7);
  assert(a->status == 200 && a->response->length == 108866 && allocated);
  assert(a->response->headers["Cache-Control"] == "no-store" && a->response->headers["X-LCD-Frame"] == "7");
  pixels[0] = 0;
  uint8_t part[17];
  assert(a->response->fill(part, 2, 66) == 2 && part[0] == 0 && part[1] == 0xf8); // immutable snapshot
  assert(a->response->fill(part, 17, 0) == 17 && part[0] == 'B' && part[1] == 'M');
  assert(a->response->fill(part, 17, 108866) == 0);
  b = request();
  assert(b->status == 429);
  a.reset();
  assert(!allocated);
  a = request();
  LocalUi::capturePoll(nullptr, true, 0, 0);
  assert(a->status == 503);
  allocateOK = false;
  a = request();
  LocalUi::capturePoll(pixels.data(), true, 32, 8);
  assert(a->status == 503 && !allocated);
  allocateOK = true;
  a = request();
  testNow = 1600;
  LocalUi::capturePoll(pixels.data(), false, 32, 9);
  assert(a->status == 503);
  failResponse = true;
  a = request();
  LocalUi::capturePoll(pixels.data(), true, 32, 10);
  assert(a->status == 503 && !allocated);
  uint8_t header[66];
  LcdBitmap::header(header);
  assert(header[18] == 0x40 && header[19] == 1 && header[22] == 0x56 && header[23] == 0xff &&
         header[28] == 16 && header[30] == 3);
  puts("Capture: completed-frame gate, immutable PSRAM copy, one in flight, disconnect, timeout and "
       "allocation failures passed.");
}

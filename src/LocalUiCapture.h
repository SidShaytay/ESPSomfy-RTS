#pragma once
#ifdef ESPSOMFY_LOCAL_UI
#include <stdint.h>
class AsyncWebServer;
namespace LocalUi {
void registerCapture(AsyncWebServer &);
// Called only by the framebuffer's owning loop, never from async_tcp.
void capturePoll(const uint16_t *pixels, bool fullyPresented, uint8_t brightness, uint32_t revision);
} // namespace LocalUi
#endif

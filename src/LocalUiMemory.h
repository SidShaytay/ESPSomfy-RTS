#pragma once
#include <stddef.h>
#include <stdint.h>
namespace LocalUi {
constexpr size_t graphicsBytes = 320 * 170 * sizeof(uint16_t);
enum class GraphicsFailure : uint8_t { none, missing, allocation };
struct GraphicsMemory {
  uint16_t *pixels = nullptr;
  GraphicsFailure failure = GraphicsFailure::none;
  void begin(bool psramAvailable, void *(*allocate)(size_t)) {
    if(!psramAvailable) {
      failure = GraphicsFailure::missing;
      return;
    }
    pixels = static_cast<uint16_t *>(allocate(graphicsBytes));
    if(!pixels)
      failure = GraphicsFailure::allocation;
  }
};
} // namespace LocalUi

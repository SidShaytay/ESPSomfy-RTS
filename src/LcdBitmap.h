#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace LcdBitmap {
constexpr size_t headerBytes = 66, pixelBytes = 320 * 170 * 2, totalBytes = headerBytes + pixelBytes;
inline void header(uint8_t *out) {
  memset(out, 0, headerBytes);
  auto word = [&](size_t at, uint32_t value, size_t bytes) {
    for(size_t i = 0; i < bytes; i++)
      out[at + i] = uint8_t(value >> (i * 8));
  };
  out[0] = 'B';
  out[1] = 'M';
  word(2, totalBytes, 4);
  word(10, headerBytes, 4);
  word(14, 40, 4);
  word(18, 320, 4);
  word(22, uint32_t(-170), 4);
  word(26, 1, 2);
  word(28, 16, 2);
  word(30, 3, 4);
  word(34, pixelBytes, 4);
  word(54, 0xf800, 4);
  word(58, 0x07e0, 4);
  word(62, 0x001f, 4);
}
} // namespace LcdBitmap

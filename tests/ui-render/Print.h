#pragma once
#include <stddef.h>
#include <stdint.h>
class Print {
public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t) = 0;
  size_t write(const uint8_t *p, size_t n) {
    size_t out = 0;
    while(n--)
      out += write(*p++);
    return out;
  }
  size_t print(const char *s) {
    size_t n = 0;
    while(*s)
      n += write(uint8_t(*s++));
    return n;
  }
};

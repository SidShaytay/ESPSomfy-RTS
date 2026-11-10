#pragma once
#include <stddef.h>
#include <stdint.h>
#include <functional>
constexpr size_t UPDATE_SIZE_UNKNOWN = ~size_t(0);
constexpr int U_SPIFFS = 100, U_FLASH=0;
struct FakeUpdate {
  bool beginOk = true, endOk = true, error = false, running = false,
       shortWrite = false;
  std::function<void()> onBegin,onWrite;
  unsigned begins = 0, writes = 0, ends = 0, aborts = 0;
  bool begin(size_t, int) {
    ++begins;
    if(onBegin) onBegin();
    // A failed begin can mean another request already owns the SDK object.
    if (!beginOk)
      return false;
    running = true;
    return true;
  }
  size_t write(uint8_t *, size_t count) {
    ++writes;
    if(onWrite) onWrite();
    if (shortWrite) {
      error = true;
      return count ? count - 1 : 0;
    }
    return count;
  }
  bool end(bool) {
    ++ends;
    running = false;
    return endOk;
  }
  bool hasError() const { return error; }
  bool isRunning() const { return running; }
  void abort() {
    ++aborts;
    error = true;
    running = false;
  }
};
extern FakeUpdate Update;

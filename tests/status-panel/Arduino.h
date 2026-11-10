#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <cassert>
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define OUTPUT 3
extern uint32_t fakeNow;
extern int pins[64];
extern bool attachOK;
inline uint32_t millis() { return fakeNow; }
inline uint32_t micros() { return fakeNow * 1000; }
inline void delay(uint32_t n) { fakeNow += n; }
inline void pinMode(int, int) {}
inline void digitalWrite(int p, int v) { pins[p] = v; }
inline int digitalRead(int p) { return pins[p]; }
inline bool ledcAttach(int, int, int) { return attachOK; }
inline bool ledcWrite(int, int v) { pins[21] = v; return true; }
extern bool owned, updaterSampling;
struct SerialStub {
  std::string output;
  template<class... T> void printf(const char *format, T... values) {
    assert(!owned && !updaterSampling);
    char text[768];
    const int count=snprintf(text,sizeof(text),format,values...);
    assert(count>=0 && size_t(count)<sizeof(text));
    output+=text;
  }
};
extern SerialStub Serial;

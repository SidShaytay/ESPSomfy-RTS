#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
using byte = uint8_t;
#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1
extern int pins[64];
inline void pinMode(uint8_t p, int) { if(p != 12) __builtin_trap(); }
inline void digitalWrite(uint8_t p, int v) { pins[p] = v; }
inline int digitalRead(uint8_t p) { return pins[p]; }
inline unsigned micros() { return 0; }
inline void delay(unsigned) {}
inline long map(long x, long a, long b, long c, long d) { return (x-a)*(d-c)/(b-a)+c; }
#define bitRead(value, bit) (((value) >> (bit)) & 1)

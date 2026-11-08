#pragma once
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
constexpr int WL_CONNECTED=3;
struct AddressText { void toCharArray(char *out,size_t size) const { snprintf(out,size,"192.0.2.1"); } };
struct Address { explicit operator uint32_t() const { return 0; } AddressText toString() const { return {}; } };
struct Wifi { int status() const { return 0; } Address localIP() const { return {}; } Address softAPIP() const { return {}; } };
inline Wifi WiFi;

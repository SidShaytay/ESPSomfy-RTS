#pragma once
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
constexpr int WL_CONNECTED=3;
struct AddressText { void toCharArray(char *out,size_t size) const { snprintf(out,size,"192.0.2.1"); } };
struct Address { uint32_t value=0;explicit operator uint32_t() const { return value; } AddressText toString() const { return {}; } };
struct WifiName {const char *c_str()const{return "Test network";}};
struct Wifi {WifiName SSID()const{return {};}int RSSI()const{return -55;} int currentStatus=0;int status() const { return currentStatus; } Address localIP() const { return {currentStatus==WL_CONNECTED?0xc0000201u:0}; } Address softAPIP() const { return {}; } };
inline Wifi WiFi;

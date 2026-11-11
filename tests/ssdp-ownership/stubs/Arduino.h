#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <functional>
#include <cmath>
using String=std::string;
using std::isnan;
struct Print{};struct WiFiClient:Print{};struct hw_timer_t{};
unsigned long millis();long random(long,long);
inline size_t strlcpy(char *d,const char *s,size_t n){size_t k=strlen(s);if(n){size_t c=k<n-1?k:n-1;memcpy(d,s,c);d[c]=0;}return k;}
#define INADDR_NONE 0xffffffffu

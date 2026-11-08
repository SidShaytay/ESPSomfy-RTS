#pragma once
#include "Arduino.h"
#include <assert.h>
#include <functional>
#include <map>
#include <string>
constexpr int HTTP_POST = 2;
struct AsyncURIMatcher {
  static const char *exact(const char *s) { return s; }
};
struct AsyncWebServerRequest {
  bool allowed = true;
  int status = 0;
  std::string body;
  std::map<std::string, bool> attributes;
  std::function<void()> disconnected;
  void setAttribute(const char *name, bool value) { attributes[name] = value; }
  bool getAttribute(const char *name, bool fallback) const {
    auto found = attributes.find(name);
    return found == attributes.end() ? fallback : found->second;
  }
  void onDisconnect(std::function<void()> callback) { disconnected = callback; }
  void send(int code, const char *, const char *json) {
    status = code;
    body = json;
  }
};
struct AsyncWebServer {
  std::function<void(AsyncWebServerRequest *)> complete;
  std::function<void(AsyncWebServerRequest *, String, size_t, uint8_t *, size_t,
                     bool)>
      upload;
  void on(const char *path, int method, decltype(complete) c,
          decltype(upload) u) {
    assert(std::string(path) == "/updateApplication" && method == HTTP_POST);
    complete = c;
    upload = u;
  }
};

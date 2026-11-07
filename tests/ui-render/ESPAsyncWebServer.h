#pragma once
#include "Arduino.h"
#include <memory>
#include <functional>
#include <map>
#include <assert.h>
constexpr int HTTP_GET = 1;
extern bool failResponse;
struct AsyncURIMatcher {
  static const char *exact(const char *s) { return s; }
};
using Filler = std::function<size_t(uint8_t *, size_t, size_t)>;
class AsyncWebServerResponse {
public:
  Filler fill;
  std::map<std::string, std::string> headers;
  size_t length;
  AsyncWebServerResponse(size_t n, Filler f) : fill(f), length(n) {}
  void addHeader(const char *key, const char *value) { headers[key] = value; }
  void addHeader(const char *key, const String &value) { headers[key] = value.c_str(); }
};
class AsyncWebServerRequest : public std::enable_shared_from_this<AsyncWebServerRequest> {
public:
  int status = 0;
  std::unique_ptr<AsyncWebServerResponse> response;
  std::weak_ptr<AsyncWebServerRequest> pause() { return shared_from_this(); }
  void send(int code, const char *, const char *) { status = code; }
  void send(AsyncWebServerResponse *r) {
    response.reset(r);
    status = 200;
  }
  AsyncWebServerResponse *beginResponse(const char *type, size_t n, Filler f) {
    assert(std::string(type) == "image/bmp");
    return failResponse ? nullptr : new AsyncWebServerResponse(n, f);
  }
};
using AsyncWebServerRequestPtr = std::weak_ptr<AsyncWebServerRequest>;
class AsyncWebServer {
public:
  std::function<void(AsyncWebServerRequest *)> handler;
  void on(const char *path, int method, std::function<void(AsyncWebServerRequest *)> h) {
    assert(std::string(path) == "/lcd/screenshot" && method == HTTP_GET);
    handler = h;
  }
};

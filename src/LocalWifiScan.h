#pragma once
#include "LocalEntityView.h"
#include "LocalUiAuthorizationAdapter.h"
#include "WifiScanFeed.h"
#include <ArduinoJson.h>
namespace LocalUi {
namespace WifiScan {
class Backend {
public:
  virtual ~Backend() = default;
  virtual SecuritySnapshot authorization() = 0;
  virtual uint64_t networkRevision() = 0;
  virtual uint64_t now() = 0;
  virtual void activity() = 0;
  virtual WifiScanFeed &feed() = 0;
};
class Form {
  struct Session;
  Backend &backend;
  ArduinoJson::Allocator &allocator;
  Session *session = nullptr;
  EntityAdmin::Notice notice = EntityAdmin::Notice::none;
  void close(EntityAdmin::Notice);
  bool guard(uint64_t);
  void touch(uint64_t);
  static void completed(void *, uint64_t, bool, const WifiScanReader &);

public:
  Form(Backend &b, ArduinoJson::Allocator &a) : backend(b), allocator(a) {}
  ~Form() { discard(); }
  Form(const Form &) = delete;
  Form &operator=(const Form &) = delete;
  bool open(uint64_t);
  void tick(uint64_t now) { guard(now); }
  void turn(int, uint64_t);
  void activate(uint64_t);
  void back(uint64_t);
  void discard() { close(EntityAdmin::Notice::none); }
  bool closed() const { return !session; }
  bool takeSSID(char (&)[33], uint64_t, SecuritySnapshot &, uint64_t &);
  void render(EntityAdmin::FormView &) const;
};
static_assert(sizeof(Form) <= 32,
              "Lazy WiFi handle; borrowed presentation frame");
Backend &runtimeBackend();
} // namespace WifiScan
} // namespace LocalUi

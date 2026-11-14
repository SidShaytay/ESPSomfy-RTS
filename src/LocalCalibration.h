#pragma once
#include "LocalEntityForm.h"
#include "LocalCatalogue.h"
#include "LiftGuideSession.h"
namespace LocalUi { namespace Calibration {
class Backend {
public:
  virtual ~Backend() = default;
  virtual EntityAdmin::Authorization authorization() = 0;
  virtual uint64_t revision() = 0;
  virtual void activity() = 0;
  virtual bool available(uint8_t id) = 0;
  virtual bool begin(uint8_t id, LiftKind, EntityAdmin::Authorization, uint32_t &token) = 0;
  virtual bool act(uint32_t token, LiftGuideAction, EntityAdmin::Authorization, bool human) = 0;
  virtual bool snapshot(uint32_t token, EntityAdmin::Authorization, LiftGuideSnapshot &) = 0;
};
class Form {
  struct Session;
  Backend *backend;
  CatalogueSource &catalogue;
  ArduinoJson::Allocator &allocator;
  Session *session = nullptr;
  EntityAdmin::Notice status = EntityAdmin::Notice::none;
  bool guard(uint64_t);
  void close(EntityAdmin::Notice, bool cancel = true);
public:
  Form(Backend *b, CatalogueSource &c, ArduinoJson::Allocator &a) : backend(b), catalogue(c), allocator(a) {}
  ~Form() { discard(); }
  Form(const Form &) = delete;
  Form &operator=(const Form &) = delete;
  bool available() const { return backend != nullptr; }
  bool open(uint64_t now);
  void tick(uint64_t now);
  void turn(int direction, uint64_t now);
  void activate(uint64_t now, bool human = true);
  void back(uint64_t now);
  void discard() { close(EntityAdmin::Notice::none); }
  bool closed() const { return session == nullptr; }
  EntityAdmin::Notice notice() const { return status; }
  void render(EntityAdmin::FormView &);
};
static_assert(sizeof(Form) <= 40,"Calibration borrows the existing private presentation frame");
Backend &runtimeBackend();
} }

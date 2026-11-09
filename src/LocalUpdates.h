#pragma once
#include "LocalEntityView.h"
#include "LocalUiAuthorizationAdapter.h"
#include "GitUpdate.h"
#include "GitUpdateBootHealth.h"
namespace LocalUi {namespace Updates {
class Backend {
public:
  virtual ~Backend()=default;
  virtual SecuritySnapshot authorization()=0;
  virtual void activity()=0;
  virtual GitUpdateSnapshot status()=0;
  virtual GitUpdateBootStatus boot()=0;
  virtual GitUpdateRequest check(uint32_t session,uint32_t security,uint64_t &operation)=0;
  virtual GitUpdateRequest install(uint64_t candidate,uint32_t session,uint32_t security,uint64_t &operation)=0;
  virtual bool cancel(uint64_t operation,uint32_t session,uint32_t security)=0;
};
class Form {
  struct Session;
  Backend &backend;
  ArduinoJson::Allocator &allocator;
  Session *session=nullptr;
  EntityAdmin::Notice notice=EntityAdmin::Notice::none;
  void close(EntityAdmin::Notice);
  bool guard(uint64_t);
  void touch(uint64_t);
public:
  Form(Backend &b,ArduinoJson::Allocator &a):backend(b),allocator(a){}
  ~Form(){discard();}
  Form(const Form &)=delete;
  Form &operator=(const Form &)=delete;
  bool open(uint64_t);
  void discard(){close(EntityAdmin::Notice::none);}
  bool closed()const{return !session;}
  void tick(uint64_t);
  void turn(int,uint64_t);
  void activate(uint64_t);
  void back(uint64_t);
  void render(EntityAdmin::FormView &)const;
};
static_assert(sizeof(Form)<=32,"Lazy updater handle borrows existing private storage and frame");
Backend &runtimeBackend();
}}

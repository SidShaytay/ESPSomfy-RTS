#pragma once
#include "FrequencyScanSession.h"
#include "LocalEntityView.h"
#include "LocalUiAuthorizationAdapter.h"
#include <ArduinoJson.h>
namespace LocalUi {namespace RadioScan {
class Backend {
public:
 virtual ~Backend()=default;
 virtual SecuritySnapshot authorization()=0;
 virtual void activity()=0;
 virtual ::FrequencyScan::Source capture()=0;
 virtual bool begin(const ::FrequencyScan::Source &,FrequencyScanToken &)=0;
 virtual bool stop(const ::FrequencyScan::Source &,const FrequencyScanToken &)=0;
 virtual ::FrequencyScan::Snapshot status()=0;
 virtual bool peak(const ::FrequencyScan::Source &,const FrequencyScanToken &,float &)=0;
};
class Form {
 struct Session;Backend &backend;ArduinoJson::Allocator &allocator;Session *session=nullptr;
 EntityAdmin::Notice notice=EntityAdmin::Notice::none;
 void close(EntityAdmin::Notice);bool guard(uint64_t);void touch(uint64_t);
public:
 Form(Backend &b,ArduinoJson::Allocator &a):backend(b),allocator(a){}
 ~Form(){discard();}Form(const Form&)=delete;Form &operator=(const Form&)=delete;
 bool open(uint64_t);void tick(uint64_t);void turn(int,uint64_t);void activate(uint64_t);void back(uint64_t);
 void discard(){close(EntityAdmin::Notice::none);}bool closed()const{return !session;}
 bool takeFrequency(float &,SecuritySnapshot &,uint64_t &radioRevision,uint64_t now);
 void render(EntityAdmin::FormView &)const;
};
static_assert(sizeof(Form)<=32,"Radio scan borrows the existing System frame");
Backend &runtimeBackend();
}}

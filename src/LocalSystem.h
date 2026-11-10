#pragma once
#include "LocalEntityView.h"
#include "LocalUiAuthorizationAdapter.h"
#include "RestartTypes.h"
namespace LocalUi {
bool readLocalNetworkRow(uint8_t,EntityAdmin::FormRow &);
constexpr uint8_t networkDetailRows=9;
namespace System {
class Backend {
public:
  virtual ~Backend()=default;
  virtual bool networkRow(uint8_t,EntityAdmin::FormRow &){return false;}
  virtual SecuritySnapshot authorization()=0;
  virtual void activity()=0;
  virtual RestartResult restart(uint32_t session,uint32_t configuration)=0;
  virtual RestartSnapshot status()=0;
};
// Loop-owned navigation. Runtime supplies the existing shared presentation
// frame; this state owns neither another frame nor any dynamic allocation.
class Form {
  Backend &backend;
  enum class Page : uint8_t { closed, menu, confirm, files, guidance, result, wifi, diagnostics, network, radio } page=Page::closed;
  uint32_t generation=0,configurationGeneration=0;
  uint64_t observedAt=0,activityAt=0,restartGeneration=0;
  uint8_t cursor=0,file=0;
  EntityAdmin::Notice notice=EntityAdmin::Notice::none;
  RestartReason reason=RestartReason::none;
  bool admitted=false,stopped=false,statusUnknown=false;
  bool guard(uint64_t);
  void touch(uint64_t);
public:
  explicit Form(Backend &value):backend(value){}
  bool open(uint64_t);
  void discard();
  bool closed() const {return page==Page::closed;}
  void tick(uint64_t);
  void turn(int,uint64_t);
  void activate(uint64_t);
  void back(uint64_t);
  void render(EntityAdmin::FormView &) const;
  bool wantsWifiScan() const {return page==Page::wifi;}
  bool wantsDiagnostics() const {return page==Page::diagnostics;}
  bool wantsRadioScan() const {return page==Page::radio;}
  void finishChild(){if(page==Page::wifi || page==Page::diagnostics || page==Page::radio){cursor=page==Page::wifi?2:page==Page::diagnostics?3:5;page=Page::menu;}}
  void childActivity(uint64_t now){if(guard(now))touch(now);}
};
static_assert(sizeof(Form)<=80,"System borrows presentation storage and allocates nothing");
Backend &runtimeBackend();
} }

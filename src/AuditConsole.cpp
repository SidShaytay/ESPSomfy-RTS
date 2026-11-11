#include "AuditConsole.h"
#ifdef ESPSOMFY_AUDIT_CONTROL
#include "AuditConsoleProtocol.h"
#ifdef AUDIT_CONSOLE_HOST_TEST
#include "../tests/audit-console/Runtime.h"
#else
#include "GitUpdateController.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LocalOperationAdmission.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp32-hal-log.h>
#endif
namespace AuditConsole {
namespace {
Parser parser;
UiInput injected;
bool active=false,report=true;
uint32_t touched=0,lastWifi=0;
uint8_t previous[5]={255,255,255,255,255};
void execute(const Request &r){
 unsigned result=0;
 const auto auth=LocalUi::pollAuthorization();
 if(r.command==Command::invalid)result=1;
 else if(r.command==Command::state){
  report=true;
  const auto update=gitUpdate.snapshot();
  log_printf("AUDIT_UPDATE schema=1 phase=%u reason=%u busy=%u progress=%u candidateReady=%u uptimeMs=%lu\n",
   unsigned(update.phase),unsigned(update.reason),unsigned(update.busy),unsigned(update.progress),
   unsigned(update.compatible),(unsigned long)millis());
 }
 else if(r.command==Command::release){injected={};active=false;}
 else if(r.command==Command::turn){active=true;injected.turn=r.turn;touched=millis();}
 else if(r.command==Command::down||r.command==Command::up){active=true;injected.pressed=r.command==Command::down;touched=millis();}
 else if(r.command==Command::backDown||r.command==Command::backUp){active=true;injected.back=r.command==Command::backDown;touched=millis();}
 else if(!auth.allowed||LocalUi::localControlsFrozen())result=2;
 else if(r.command==Command::wifi){
  if(WiFi.status()!=WL_CONNECTED || (lastWifi && millis()-lastWifi<30000))result=3;
  else {lastWifi=millis();result=WiFi.disconnect(false,false)?0:3;}
 }else if(r.command==Command::check){
  uint64_t operation=0;
  result=unsigned(requestLocalGitUpdateCheck(auth.generation,auth.configurationGeneration,operation));
 }else if(r.command==Command::stage)result=unsigned(gitUpdate.requestAuditStage(gitUpdate.snapshot().candidate));
 else if(r.command==Command::cancel)result=gitUpdate.requestCancel()?0:3;
 log_printf("AUDIT_ACK schema=1 id=%lu command=%u result=%u uptimeMs=%lu\n",
  (unsigned long)r.id,unsigned(r.command),result,(unsigned long)millis());
}
}
void loop(){
 if(active && uint32_t(millis()-touched)>=2000){injected={};active=false;}
 Request r;
 // At most one line and64 bytes per pass; no readString/wait/heap allocation.
 for(unsigned n=0;n<64 && Serial.available()>0;++n)
  if(parser.feed(char(Serial.read()),r)){execute(r);break;}
}
bool uiInput(UiInput &out){
 if(!active)return false;
 out=injected;injected.turn=0;return true;
}
void uiState(uint8_t screen,uint8_t selected,uint8_t mode,uint8_t formSelected,bool visible){
 const uint8_t now[]={screen,selected,mode,formSelected,uint8_t(visible)};
 if(!report && !memcmp(now,previous,sizeof(now)))return;
 memcpy(previous,now,sizeof(now));report=false;
 log_printf("AUDIT_UI schema=1 screen=%u selected=%u mode=%u formSelected=%u visible=%u uptimeMs=%lu\n",
  unsigned(screen),unsigned(selected),unsigned(mode),unsigned(formSelected),unsigned(visible),(unsigned long)millis());
}
}
#endif

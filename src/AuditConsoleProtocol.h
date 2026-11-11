#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace AuditConsole {
enum class Command:uint8_t {invalid,state,turn,down,up,backDown,backUp,release,wifi,check,stage,cancel};
struct Request {uint32_t id=0;Command command=Command::invalid;int8_t turn=0;};
// Loop-owned, bounded ASCII lines. Bad/oversized lines are drained completely;
// accepted sequence IDs cannot be replayed in the same boot.
class Parser {
 char bytes[64]={};size_t used=0;bool bad=false;uint32_t previous=0;
 Request parse(){
  Request r;
  if(bad || strncmp(bytes,"AUDIT ",6))return r;
  const char *p=bytes+6;
  if(*p<'1'||*p>'9')return r;
  uint32_t id=0;
  while(*p>='0'&&*p<='9'){
   const uint32_t digit=uint32_t(*p++-'0');
   if(id>(UINT32_MAX-digit)/10)return r;
   id=id*10+digit;
  }
  if(*p++!=' '||id<=previous)return r;
  struct Entry {const char *text;Command value;};
  const Entry entries[]={{"state",Command::state},{"turn -1",Command::turn},{"turn 1",Command::turn},
   {"down",Command::down},{"up",Command::up},{"back-down",Command::backDown},{"back-up",Command::backUp},
   {"release",Command::release},{"wifi-reconnect",Command::wifi},{"update-check",Command::check},
   {"update-stage",Command::stage},{"update-cancel",Command::cancel}};
  for(const auto &entry:entries)if(!strcmp(p,entry.text)){
   previous=r.id=id;r.command=entry.value;
   if(r.command==Command::turn)r.turn=p[5]=='-'?-1:1;
   return r;
  }
  return r;
 }
public:
 bool feed(char c,Request &out){
  if(c=='\r')return false;
  if(c=='\n'){out=parse();used=0;bad=false;bytes[0]=0;return true;}
  if(c<32||c>126||used==sizeof(bytes)-1){bad=true;return false;}
  if(!bad){bytes[used++]=c;bytes[used]=0;}
  return false;
 }
};
}

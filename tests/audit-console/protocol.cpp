#include "AuditConsoleProtocol.h"
#include <cassert>
#include <string>
using namespace AuditConsole;
Request line(Parser &p,const std::string &s){Request r;for(char c:s)if(p.feed(c,r))return r;return {};}
int main(){
 Parser p;
 auto r=line(p,"AUDIT 1 state\n");assert(r.id==1&&r.command==Command::state);
 r=line(p,"AUDIT 2 turn -1\r\n");assert(r.id==2&&r.command==Command::turn&&r.turn==-1);
 assert(line(p,"AUDIT 2 down\n").command==Command::invalid); // Replay has no effect.
 assert(line(p,"AUDIT 3 turn 127\n").command==Command::invalid);
 assert(line(p,"AUDIT 4 state extra\n").command==Command::invalid);
 assert(line(p,"AUDIT 5 update-stage\n").command==Command::stage);
 assert(line(p,"AUDIT 6 update-install\n").command==Command::invalid);
 assert(line(p,"AUDIT 7 open 1\n").command==Command::invalid);
 assert(line(p,"AUDIT 4294967296 state\n").command==Command::invalid);
 assert(line(p,"AUDIT 8 "+std::string(90,'x')+"\n").command==Command::invalid);
 assert(line(p,"AUDIT 9 wifi-reconnect\n").command==Command::wifi);
 assert(line(p,std::string("AUDIT 10 sta\0te\n",17)).command==Command::invalid);
 assert(line(p,"AUDIT 11 release\n").command==Command::release);
}

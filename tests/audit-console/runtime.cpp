#include "../../src/AuditConsole.cpp"
#include <cassert>
static void send(const std::string &s){for(char c:s)Serial.bytes.push_back(c);while(Serial.available())AuditConsole::loop();}
int main(){
 AuditConsole::UiInput input;
 assert(!AuditConsole::uiInput(input));
 send("AUDIT 1 turn 1\n");assert(AuditConsole::uiInput(input)&&input.turn==1);
 assert(AuditConsole::uiInput(input)&&input.turn==0);
 send("AUDIT 2 down\n");assert(AuditConsole::uiInput(input)&&input.pressed);
 now+=2000;AuditConsole::loop();assert(!AuditConsole::uiInput(input));
 LocalUi::security.allowed=false;
 send("AUDIT 3 wifi-reconnect\nAUDIT 4 update-check\nAUDIT 5 update-stage\n");
 assert(WiFi.disconnects==0&&checks==0&&gitUpdate.stages==0);
 LocalUi::security.allowed=true;
 send("AUDIT 6 wifi-reconnect\nAUDIT 7 wifi-reconnect\n");assert(WiFi.disconnects==1);
 send("AUDIT 8 update-check\nAUDIT 9 update-stage\nAUDIT 10 update-cancel\n");assert(checks==1&&gitUpdate.stages==1&&gitUpdate.cancels==1);
 send("AUDIT 9 update-stage\n");assert(gitUpdate.stages==1);
 LocalUi::frozen=true;send("AUDIT 11 update-stage\n");assert(gitUpdate.stages==1);
 send("AUDIT 12 turn -1\nAUDIT 13 release\n");assert(!AuditConsole::uiInput(input));
 AuditConsole::uiState(0,1,2,3,true);auto size=logs.size();AuditConsole::uiState(0,1,2,3,true);assert(logs.size()==size);
 send("AUDIT 14 state\n");AuditConsole::uiState(0,1,2,3,true);assert(logs.size()==size+3);
}

#pragma once
#include "WiFi.h"
struct AsyncUDPPacket {
 std::string bytes;size_t cursor=0;IPAddress remote;uint16_t port;
 AsyncUDPPacket(std::string b,IPAddress a,uint16_t p):bytes(b),remote(a),port(p){}
 size_t length(){return bytes.size();}size_t available(){return bytes.size()-cursor;}
 char read(){return bytes[cursor++];}IPAddress remoteIP(){return remote;}uint16_t remotePort(){return port;}
};
struct AsyncUDP {
 bool open=false;
 bool connected(){return open;}operator bool()const{return open;}
 void close(){open=false;}
 template<class F>void onPacket(F,void*){}
 bool listenMulticast(IPAddress,uint16_t){open=true;return true;}
};

#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
using String=std::string;
constexpr int HTTP_GET=1,HTTP_POST=2;
struct AsyncWebServerResponse {
 int code=200;String mime;const uint8_t *data=nullptr;size_t size=0;
 std::map<String,String> headers;
 void addHeader(const char *key,const char *value){headers[key]=value;}
};
struct AsyncWebServerRequest {
 String path="/";int verb=HTTP_GET;bool http=true,aborted=false;String incomingEtag;
 AsyncWebServerResponse result;
 bool isHTTP(){return http;}int method(){return verb;}String url(){return path;}
 String header(const char *){return incomingEtag;}
 AsyncWebServerResponse *beginResponse(int code,const char *mime="",const uint8_t *data=nullptr,size_t size=0){
  auto *r=new AsyncWebServerResponse;r->code=code;r->mime=mime;r->data=data;r->size=size;return r;
 }
 void send(AsyncWebServerResponse *response){result=*response;delete response;}
 void send(int code){result.code=code;}
 void abort(){aborted=true;}
};
struct AsyncWebHandler {
 virtual ~AsyncWebHandler()=default;
 virtual bool canHandle(AsyncWebServerRequest *)const{return false;}
 virtual void handleRequest(AsyncWebServerRequest *){}
};
struct AsyncWebServer {
 AsyncWebHandler *handler=nullptr;
 ~AsyncWebServer(){delete handler;}
 void addHandler(AsyncWebHandler *h){handler=h;}
};

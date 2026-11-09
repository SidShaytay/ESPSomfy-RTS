#if defined(ESPSOMFY_BUNDLED_WEB)
#include "BundledWebRoutes.h"
#include "BundledWeb.h"
#include <ESPAsyncWebServer.h>
#include <cstdio>
#include <new>
namespace {
class BundledWebHandler final:public AsyncWebHandler {
public:
  bool canHandle(AsyncWebServerRequest *request)const override {
    return request->isHTTP() && request->method()==HTTP_GET && BundledWeb::find(request->url().c_str());
  }
  void handleRequest(AsyncWebServerRequest *request)override {
    const auto *asset=BundledWeb::find(request->url().c_str());
    if(!asset){request->send(404);return;}
    char etag[67];std::snprintf(etag,sizeof(etag),"\"%s\"",asset->sha256);
    const bool cached=request->header("If-None-Match")==etag;
    auto *response=cached?request->beginResponse(304):request->beginResponse(200,asset->mime,asset->data,asset->size);
    if(!response){request->abort();return;}
    response->addHeader("Cache-Control","no-cache");response->addHeader("ETag",etag);
    if(!cached && asset->gzip)response->addHeader("Content-Encoding","gzip");
    request->send(response);
  }
};
}
void registerBundledWebRoutes(AsyncWebServer &server){auto *handler=new(std::nothrow) BundledWebHandler();if(handler)server.addHandler(handler);}
#endif

#pragma once
#include "ManualUpdateLifecycle.h"
#include <cstddef>
class AsyncWebServerRequest;
void receivePreparedApplication(AsyncWebServerRequest *,const GitUpdateOwnerToken &,const ManualApplicationSource &,
                                size_t index,uint8_t *data,size_t len,bool final);
// Called after the complete authenticated multipart request, not file-final.
bool finishPreparedApplication(AsyncWebServerRequest *,const GitUpdateOwnerToken &,const ManualApplicationSource &);

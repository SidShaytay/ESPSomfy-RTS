#pragma once
#include "GitUpdate.h"
#include "ManualUpdateLifecycle.h"
#include "RestartTypes.h"
#include <stddef.h>
class AsyncWebServerRequest;
struct ManualUploadState {
  GitUpdateOwnerToken owner;
  size_t received=0;
  uint64_t securityRevision=0,operation=0,applicationNonce=0;
  uint32_t expectedBytes=0;
  ManualUpdateKind kind=ManualUpdateKind::firmware;
  bool started=false,failed=false,uploaded=false,finalReceived=false;
  bool applicationAdopted=false;
  bool modelAttempted=false,modelQueued=false,restartQueued=false;
  RestartReason restartReason=RestartReason::none;
};
static_assert(sizeof(ManualUploadState)<=64,"Keep request upload metadata bounded");
ManualUploadState *manualUploadState(AsyncWebServerRequest *request);
void receiveManualImage(AsyncWebServerRequest *request,ManualUpdateKind kind,size_t index,
                        uint8_t *data,size_t len,bool final);
void receiveManualRestore(AsyncWebServerRequest *,size_t index,uint8_t *data,size_t len,bool final);
// Completion/discard require an owned receive operation claim; queue handoff
// retires that claim before publishing its FIFO marker.
bool claimManualRestore(AsyncWebServerRequest *);
void discardManualRestore(AsyncWebServerRequest *);
bool queueManualRestore(AsyncWebServerRequest *,uint8_t parts);
#if defined(ESPSOMFY_BUNDLED_WEB)
bool queueManualFirmware(AsyncWebServerRequest *);
void runManualFirmware(const GitUpdateOwnerToken &);
#else
inline void runManualFirmware(const GitUpdateOwnerToken &){}
#endif
// Poll on loop(), including abandoned requests. Cleanup claims exclude an
// in-flight SDK/file call and can never touch a newer owner's resource.
void manualUploadLoop();

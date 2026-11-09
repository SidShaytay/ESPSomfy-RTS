#include "ESPAsyncWebServer.h"
#include "ModelCommitService.h"
#include "SafeRestart.h"
#include "SomfyFrame.h"
#include "LoopQueue.h"
#include "Update.h"
#include "Utils.h"
#include "esp_timer.h"
#include <iostream>
#include <vector>
#define SOMFY_H
#define webserver_h
#define MQTT_CONNECTION_H
#define TASK_WDT_H
struct SomfyShadeController {
  struct Radio {
    bool uncertain=false;
    bool counterEditUncertain() const{return uncertain;}
    bool sealed=false;
    bool restartQuiescent() const{return true;}
    bool sealForRestart(){sealed=true;return true;}
    void reopenAfterRefusedRestart(){sealed=false;}
    bool txSuspended()const{return stops!=0;} bool txHasRoom()const{return true;}
    unsigned stops = 0;
    void end() { ++stops; }
  } transceiver;
  uint64_t configurationRevision=1;
  bool isDirty=false;
  struct Shade {uint8_t id=255;uint8_t getShadeId(){return id;}bool idleForAdministration(){return true;}} shades[32];
  struct Group {uint8_t id=255;int8_t direction=0;somfy_frame_t lastFrame;uint8_t linkedShades[32]{};uint8_t getGroupId(){return id;}} groups[16];
  Shade *getShadeById(uint8_t id){for(auto &shade:shades)if(id && id!=255 && shade.id==id)return &shade;return nullptr;}
  ModelCommitResult commit(){noteModelCommit({ModelCommitState::saved});return {ModelCommitState::saved};}
} somfy;
struct ConfigSettings { bool hasOtaSlot=true; } settings;
#define configsettings_h
struct Web {
  bool uploadSuccess = false;
} webServer;
struct MqttConnection {
  unsigned stops = 0;
  void end() { ++stops; }
} mqtt;
FakeUpdate Update;

static bool admitted = true;
static std::vector<bool> restarts;
static unsigned resets = 0;
static bool restoreValid=true,restoreQueue=true;
static unsigned validations=0,restores=0;
static bool expireDuringValidation=false;
static std::function<void()> duringValidation;
static loop_item_t restoreMarker;
#define configfile_h
#define F(value) value
struct restore_options_t {
  bool settings=false,shades=false,network=false,transceiver=false,repeaters=false,mqtt=false;
  void fromJSON(JsonObject &obj);
};
struct ShadeConfigFile {
  static bool validateFile(const char*){++validations;if(duringValidation)duringValidation();if(expireDuringValidation)manualClockMs+=30001;return restoreValid;}
  static bool restore(SomfyShadeController*,const char*,restore_options_t&,const GitUpdateOwnerToken &owner) {
    assert(gitUpdate.transferRestoreClaimed(owner,manualClockMs));
    if(somfy.transceiver.uncertain)return false;
    ++restores;return restoreValid;
  }
};
struct RecordingRequests {shade_request_result_t run(const shade_request_t&){return shade_request_result_t::done;}} shadeRequests;
#define LOCAL_QUEUE_EXTERNAL_REBOOT
#define LOCAL_QUEUE_EXTERNAL_CONFIG_FILE
#define LOCAL_QUEUE_REAL_UPDATE_OWNER
#include "../ui-adapter/LoopHarness.h"
bool asyncHasParam(AsyncWebServerRequest *request,const char*){return request->hasRestoreOptions;}
String asyncParam(AsyncWebServerRequest *request,const char*){return String(request->restoreOptions);}
bool queueLoopItem(AsyncWebServerRequest *request,const loop_item_t &item){
  restoreMarker=item;
  const bool queued=restoreQueue && loopQueue.submit(item);
  if(!queued)request->send(503,"application/json","{\"status\":\"ERROR\"}");
  return queued;
}
#include "../../src/SafeRestart.cpp"
void performLegacyShutdown(){assert(false);}
#include "restore-options.inc"
static void resetTaskWdt() { ++resets; }
const char _encoding_json[] = "application/json";
bool uploadAllowed(AsyncWebServerRequest *request, size_t) {
  return request->allowed;
}
bool submitModelCommit(const proto_reply_to_t &, ModelCommitMode mode,const GitUpdateOwnerToken &owner) {
  assert(owner.generation && gitUpdate.ownsExternal(owner));
  assert(gitUpdate.claimTransferRecovery(owner,manualClockMs));
  assert(gitUpdate.finishTransfer(owner,manualClockMs,false));
  assert(mode != ModelCommitMode::save);
  restarts.push_back(mode == ModelCommitMode::applicationRecoveryAndRestart);
  return admitted;
}
#include "../../src/entryPoints/ManualUploadSession.cpp"
#include "../../src/entryPoints/ApplicationUpload.cpp"
#include "../../src/entryPoints/FirmwareUpload.cpp"
#include "../../src/entryPoints/RestoreUpload.cpp"
static void reset() {
  cancelRestart();restartLoop();
  manualClockMs+=60000;manualUploadLoop();loopQueue.drain();
  settings.hasOtaSlot=true;
  Update = {};
  somfy = {};
  mqtt = {};
  webServer = {};
  admitted = true;
  restarts.clear();
  LittleFS={};restoreValid=restoreQueue=true;validations=restores=0;restoreMarker={};
  resets = 0;restartLoop();
}
int main() {
  AsyncWebServer server;
  registerApplicationUploadRoute(server);
  uint8_t byte = 42;loopQueue.begin();
  reset();
#if defined(ESPSOMFY_BUNDLED_WEB)
  GitUpdateOwnerToken competing;
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,competing));
  AsyncWebServerRequest refused;
  server.upload(&refused,String("retired.bin"),0,&byte,1,false);
  assert(refused.status==410 && refused.responses==1 && !refused._tempObject);
  assert(!refused.getAttribute("manualAttempted",false));
  server.upload(&refused,String("retired.bin"),1,&byte,1,true);server.complete(&refused);
  assert(refused.responses==1 && !Update.begins && !Update.writes && !Update.ends && !Update.aborts);
  assert(!LittleFS.opens && !LittleFS.writes && !LittleFS.removes && !somfy.transceiver.stops && !mqtt.stops);
  assert(restarts.empty() && !restartPending() && gitUpdate.ownsExternal(competing));
  assert(gitUpdate.releaseExternal(competing));
  AsyncWebServerRequest empty;server.complete(&empty);assert(empty.status==410 && !gitUpdate.busy());
  std::cout << "Bundled application upload retired before owner, SDK, filesystem or shutdown effects.\n";return 0;
#endif
  AsyncWebServerRequest first, overlap;
  server.upload(&first,String("a.bin"),0,&byte,1,false);
  server.upload(&overlap,String("b.bin"),0,&byte,1,false);
  assert(Update.begins==1 && Update.writes==1); // Shared SDK has one request owner.
  first.disconnected();
  reset();
  somfy.transceiver.uncertain=true;restartLoop();
  AsyncWebServerRequest uncertain;
  server.upload(&uncertain,String("x.bin"),0,&byte,1,true);server.complete(&uncertain);
  assert(Update.begins==0 && somfy.transceiver.stops==0 && uncertain.status==503);
  assert(!gitUpdate.busy());
  reset();
  Update.onBegin=[] {
    GitUpdateOwnerToken counter;
    assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::counter,counter));
  };
  AsyncWebServerRequest held;
  server.upload(&held,String("x.bin"),0,&byte,1,true);
  GitUpdateOwnerToken counter;
  assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::counter,counter));
  held.disconnected(); // Successful OTA retains owner through restart admission.
  assert(gitUpdate.busy());
  reset();
  Update.onWrite=[] {
    manualClockMs+=30000;manualUploadLoop();
    assert(Update.aborts==0); // Expiry cannot abort a currently claimed SDK write.
  };
  AsyncWebServerRequest expiredWrite;
  server.upload(&expiredWrite,String("x.bin"),0,&byte,1,true);server.complete(&expiredWrite);
  assert(Update.aborts==1 && Update.ends==0 && expiredWrite.status==500);
  assert(restarts.empty() && !gitUpdate.busy());
  reset();
  AsyncWebServerRequest oldRequest,newRequest;
  server.upload(&oldRequest,String("x.bin"),0,&byte,1,false);
  manualClockMs+=30000;manualUploadLoop();
  assert(Update.aborts==1 && !gitUpdate.busy());
  server.upload(&newRequest,String("x.bin"),0,&byte,1,false);
  const auto writesBeforeStale=Update.writes,abortsBeforeStale=Update.aborts;
  server.upload(&oldRequest,String("x.bin"),1,&byte,1,true);oldRequest.disconnected();
  assert(Update.writes==writesBeforeStale && Update.aborts==abortsBeforeStale && gitUpdate.busy());
  reset();
  AsyncWebServerRequest denied;
  denied.allowed = false;
  server.upload(&denied, String("x.bin"), 0, &byte, 1, true);
  server.complete(&denied);
  assert(Update.begins == 0 && restarts.empty() && denied.status == 503);
  reset();
  AsyncWebServerRequest success;
  server.upload(&success, String("x.bin"), 0, &byte, 1, true);
  server.complete(&success);
  assert(success.status == 202 && restarts == std::vector<bool>{true});
  assert(success.body.find("queued") != std::string::npos &&
         success.body.find("SUCCESS") == std::string::npos);
  success.disconnected();
  assert(restarts.size() == 1);
  reset();
  admitted = false;
  AsyncWebServerRequest full;
  server.upload(&full, String("x.bin"), 0, &byte, 1, true);
  server.complete(&full);
  assert(full.status == 503 && restarts == std::vector<bool>{true});
  full.disconnected();
  assert(restarts.size() == 1);
  reset();
  Update.beginOk = false;
  Update.running = true;
  webServer.uploadSuccess = true;
  AsyncWebServerRequest beginFailed;
  server.upload(&beginFailed, String("x.bin"), 0, &byte, 1, true);
  server.complete(&beginFailed);
  assert(beginFailed.status == 503 && restarts.empty());
  assert(Update.begins == 1 && Update.writes == 0 && Update.ends == 0 &&
         Update.aborts == 0);
  assert(Update.running && webServer.uploadSuccess && beginFailed.disconnected);
  beginFailed.disconnected(); // Stale closed token cannot abort foreign SDK state.
  assert(Update.aborts==0 && Update.running);
  assert(somfy.transceiver.stops == 0 && mqtt.stops == 0);
  Update.beginOk = true;
  server.upload(&beginFailed, String("x.bin"), 1, &byte, 1, true);
  server.upload(&beginFailed, String("x.bin"), 0, &byte, 1, true);
  assert(Update.begins == 1 && Update.writes == 0 && Update.ends == 0 &&
         Update.aborts == 0 && restarts.empty());
  reset();
  Update.endOk = false;
  AsyncWebServerRequest endFailed;
  server.upload(&endFailed, String("x.bin"), 0, &byte, 1, true);
  server.complete(&endFailed);
  assert(endFailed.status == 500 && restarts == std::vector<bool>{false});
  const auto failedWrites = Update.writes;
  Update.running = true;
  Update.error = false;
  server.upload(&endFailed, String("x.bin"), 1, &byte, 1, true);
  endFailed.disconnected();
  assert(Update.writes == failedWrites && Update.ends == 1 &&
         Update.aborts == 0 && Update.running);
  assert(restarts == std::vector<bool>{false});
  reset();
  Update.shortWrite = true;
  Update.endOk = false;
  AsyncWebServerRequest failed;
  server.upload(&failed, String("x.bin"), 0, &byte, 1, true);
  server.complete(&failed);
  assert(failed.status == 500 && restarts == std::vector<bool>{false});
  failed.disconnected();
  assert(restarts.size() == 1 && Update.aborts == 1);
  assert(Update.writes == 1 && Update.ends == 0);
  // A foreign transfer can own Update after our abort. Remaining chunks,
  // including a repeated index-zero callback, must not touch that transfer.
  Update.running = true;
  Update.error = false;
  Update.shortWrite = false;
  webServer.uploadSuccess = true;
  server.upload(&failed, String("x.bin"), 1, &byte, 1, false);
  server.upload(&failed, String("x.bin"), 2, &byte, 1, true);
  server.upload(&failed, String("x.bin"), 0, &byte, 1, true);
  failed.disconnected();
  assert(Update.begins == 1 && Update.writes == 1 && Update.ends == 0 &&
         Update.aborts == 1);
  assert(Update.running && webServer.uploadSuccess &&
         restarts == std::vector<bool>{false});
  reset();
  Update.error = true;
  AsyncWebServerRequest inconsistent;
  server.upload(&inconsistent, String("x.bin"), 0, &byte, 1, true);
  server.complete(&inconsistent);
  assert(inconsistent.status == 500 && restarts == std::vector<bool>{false});
  reset();
  AsyncWebServerRequest abandoned;
  server.upload(&abandoned, String("x.bin"), 0, &byte, 1, false);
  assert(restarts.empty());
  abandoned.disconnected();
  manualUploadLoop();
  assert(Update.aborts == 1 && restarts == std::vector<bool>{false});
  abandoned.disconnected();
  assert(restarts.size() == 1);
  reset();
  AsyncWebServer firmwareServer;registerFirmwareUploadRoute(firmwareServer);
  settings.hasOtaSlot=false;AsyncWebServerRequest unsupported;
  firmwareServer.upload(&unsupported,String("x.bin"),0,&byte,1,true);firmwareServer.complete(&unsupported);
  assert(unsupported.status==400 && !Update.begins && !restartPending());
  reset();
  AsyncWebServerRequest appActive,firmwareOverlap;
  server.upload(&appActive,String("x.bin"),0,&byte,1,false);
  firmwareServer.upload(&firmwareOverlap,String("x.bin"),0,&byte,1,true);firmwareServer.complete(&firmwareOverlap);
  assert(Update.begins==1 && firmwareOverlap.status==503 && !restartPending());
  reset();
  AsyncWebServerRequest firmwareSuccess;
  firmwareServer.upload(&firmwareSuccess,String("x.bin"),0,&byte,1,true);firmwareServer.complete(&firmwareSuccess);
  assert(firmwareSuccess.status==202 && restartPending() && gitUpdate.busy());
  assert(restarts.empty());
  firmwareSuccess.disconnected();assert(gitUpdate.busy());
  reset();
  Update.beginOk=false;Update.running=true;AsyncWebServerRequest firmwareBeginFailed;
  firmwareServer.upload(&firmwareBeginFailed,String("x.bin"),0,&byte,1,true);firmwareServer.complete(&firmwareBeginFailed);
  firmwareBeginFailed.disconnected();manualUploadLoop();
  assert(firmwareBeginFailed.status==503 && Update.running && !Update.aborts && !Update.writes && !Update.ends && !restartPending());
  reset();
  AsyncWebServerRequest firmwareOld,appNew;
  firmwareServer.upload(&firmwareOld,String("x.bin"),0,&byte,1,false);
  manualClockMs+=30000;manualUploadLoop();
  assert(Update.aborts==1 && !restartPending());
  server.upload(&appNew,String("x.bin"),0,&byte,1,false);
  const auto beforeLate=Update.writes;
  firmwareServer.upload(&firmwareOld,String("x.bin"),1,&byte,1,true);firmwareOld.disconnected();
  assert(Update.writes==beforeLate && Update.aborts==1 && !restartPending());
  reset();
  AsyncWebServer restoreServer;registerRestoreUploadRoute(restoreServer);
  AsyncWebServerRequest busyImage,busyRestore;
  server.upload(&busyImage,String("x.bin"),0,&byte,1,false);
  restoreServer.upload(&busyRestore,String("x.backup"),0,&byte,1,true);
  assert(LittleFS.opens==0 && LittleFS.writes==0);
  reset();
  AsyncWebServerRequest restoreSuccess;
  restoreServer.upload(&restoreSuccess,String("x.backup"),0,&byte,1,true);restoreServer.complete(&restoreSuccess);
  assert(restoreSuccess.status==202 && restoreMarker.externalOwner.generation && restoreMarker.externalOwner.kind==GitUpdateExternalOwner::restore);
  const auto restoreSource=restoreMarker.externalOwner;
  restoreSuccess.disconnected();assert(gitUpdate.ownsExternal(restoreSource));
  loopQueue.drain();assert(restores==1 && restartPending() && !gitUpdate.ownsExternal(restoreSource));
  assert(!LittleFS.files.count("/shades.tmp") && gitUpdate.busy());
  reset();
  AsyncWebServerRequest staleRestore,newerRestore;
  restoreServer.upload(&staleRestore,String("x.backup"),0,&byte,1,true);restoreServer.complete(&staleRestore);
  manualClockMs+=30000;manualUploadLoop();
  uint8_t newerByte=99;
  restoreServer.upload(&newerRestore,String("x.backup"),0,&newerByte,1,false);
  const auto beforeStaleRemove=LittleFS.removes;
  loopQueue.drain();restoreServer.complete(&staleRestore);staleRestore.disconnected();
  assert(restores==0 && LittleFS.removes==beforeStaleRemove && LittleFS.files["/shades.tmp"]==std::vector<uint8_t>{99});
  assert(!restartPending());
  reset();
  for(unsigned i=0;i<LOOP_QUEUE_SIZE;++i){loop_item_t read;read.type=loop_item_type_t::proto_read;assert(loopQueue.submit(read));}
  AsyncWebServerRequest fullRestore;
  restoreServer.upload(&fullRestore,String("x.backup"),0,&byte,1,true);restoreServer.complete(&fullRestore);
  assert(fullRestore.status==503 && loopQueue.pending()==LOOP_QUEUE_SIZE && !gitUpdate.busy() && !LittleFS.files.count("/shades.tmp"));
  reset();
  LittleFS.shortWrite=true;AsyncWebServerRequest shortRestore;
  restoreServer.upload(&shortRestore,String("x.backup"),0,&byte,1,true);restoreServer.complete(&shortRestore);
  assert(shortRestore.status==409 && !validations && !restores && !gitUpdate.busy());
  assert(!LittleFS.files.count("/shades.tmp"));
  reset();
  restoreValid=false;AsyncWebServerRequest invalidRestore;
  restoreServer.upload(&invalidRestore,String("x.backup"),0,&byte,1,true);restoreServer.complete(&invalidRestore);
  assert(invalidRestore.status==400 && validations==1 && !loopQueue.pending() && !gitUpdate.busy());
  reset();
  AsyncWebServerRequest emptyOptions;emptyOptions.hasRestoreOptions=true;emptyOptions.restoreOptions="{}";
  restoreServer.upload(&emptyOptions,String("x.backup"),0,&byte,1,true);restoreServer.complete(&emptyOptions);
  assert(emptyOptions.status==400 && !validations && !gitUpdate.busy());
  reset();expireDuringValidation=true;AsyncWebServerRequest slowValidation;
  restoreServer.upload(&slowValidation,String("x.backup"),0,&byte,1,true);restoreServer.complete(&slowValidation);
  assert(slowValidation.status==409 && !loopQueue.pending());
  expireDuringValidation=false;manualUploadLoop();assert(!gitUpdate.busy());
  reset();AsyncWebServerRequest disconnectedValidation;
  restoreServer.upload(&disconnectedValidation,String("x.backup"),0,&byte,1,true);
  duringValidation=[&]{disconnectedValidation.disconnected();};
  restoreServer.complete(&disconnectedValidation);duringValidation={};
  manualClockMs+=60000;manualUploadLoop();
  assert(disconnectedValidation.status==409 && !loopQueue.pending() && !gitUpdate.busy());
  reset();AsyncWebServerRequest loopExpiredValidation;
  restoreServer.upload(&loopExpiredValidation,String("x.backup"),0,&byte,1,true);
  duringValidation=[&]{manualClockMs+=30001;manualUploadLoop();};
  restoreServer.complete(&loopExpiredValidation);duringValidation={};
  manualClockMs+=60000;manualUploadLoop();
  assert(loopExpiredValidation.status==409 && !loopQueue.pending() && !gitUpdate.busy());
  std::cout << "Production manual uploads/LoopQueue: shared request owner, SDK failures, expiry, restart source, restore FIFO and stale-file protection passed; marker " << sizeof(loop_item_t) << " bytes.\n";
}

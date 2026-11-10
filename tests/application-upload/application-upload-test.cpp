#include "RestoreExecution.h"
#include "SettingsDurability.h"
#include "SettingsRevisions.h"
#include "ProtoSettings.h"
#include "ModelLoadTrust.h"
#include "ESPAsyncWebServer.h"
#include "ModelCommitService.h"
#include "SafeRestart.h"
#include "SomfyFrame.h"
#include "SettingsLoadState.h"
#include "LoopQueue.h"
#include "Update.h"
#include "Utils.h"
#include "esp_timer.h"
#if defined(ESPSOMFY_BUNDLED_WEB)
#include "esp_heap_caps.h"
// This HTTP fixture has no worker. Simulate only its public ticket reservation;
// the real worker/controller suite separately verifies reservation and adoption.
static bool reserveReady=true;
static bool overrideFirmwareStatus=false;
static GitUpdateSnapshot firmwareStatus;
extern "C" GitUpdateSnapshot __real__ZNK14GitUpdateClass8snapshotEv(const GitUpdateClass *);
extern "C" GitUpdateSnapshot __wrap__ZNK14GitUpdateClass8snapshotEv(const GitUpdateClass *service) {
  return overrideFirmwareStatus?firmwareStatus:__real__ZNK14GitUpdateClass8snapshotEv(service);
}
extern "C" bool __wrap__ZN14GitUpdateClass20reserveManualInstallERK19GitUpdateOwnerTokenRm(
    GitUpdateClass *service,const GitUpdateOwnerToken &owner,uint64_t &operation) {
  assert(service==&gitUpdate&&service->ownsExternal(owner));
  if(!reserveReady)return false;
  static uint64_t ticket=9007199254740992ULL;operation=++ticket;return true;
}
#endif
#include <iostream>
#include <vector>
#define SOMFY_H
#define webserver_h
#define MQTT_CONNECTION_H
#define TASK_WDT_H
struct SomfyShadeController {
  ModelLoadTrust modelLoad{ModelLoadStatus::loaded};
  struct Radio {
    struct {SettingsLoadStatus loadStatus{SettingsLoadState::loaded};}config;
    bool uncertain=false;
    bool counterEditUncertain() const{return uncertain;}
    bool sealed=false,restorePaused=false;uint64_t restoreGeneration=0;
    bool pauseForRestore(RadioInstallToken &out){if(sealed||uncertain||out.generation)return false;sealed=restorePaused=true;out.generation=++restoreGeneration;return true;}
    bool installPauseHeld(const RadioInstallToken &token)const{return restorePaused&&sealed&&token.generation==restoreGeneration;}
    RadioInstallResume resumeAfterInstall(const RadioInstallToken &token){if(!installPauseHeld(token))return RadioInstallResume::stale;restorePaused=sealed=false;return RadioInstallResume::resumed;}

    bool restartQuiescent() const{return true;}
    bool sealForRestart(){sealed=true;return true;}
    void reopenAfterRefusedRestart(){sealed=false;}
    bool txSuspended()const{return stops!=0;} bool txHasRoom()const{return true;}
    unsigned stops = 0;
    void end() { ++stops; }
  } transceiver;
  uint64_t configurationRevision=1;
  bool isDirty=false;
  struct Shade {radio_proto proto=radio_proto::RTS;uint8_t id=255;uint8_t getShadeId(){return id;}bool idleForAdministration(){return true;}} shades[32];
  struct Group {uint8_t id=255;int8_t direction=0;somfy_frame_t lastFrame;uint8_t linkedShades[32]{};uint8_t getGroupId(){return id;}} groups[16];
  Shade *getShadeById(uint8_t id){for(auto &shade:shades)if(id && id!=255 && shade.id==id)return &shade;return nullptr;}
  ModelCommitResult commit(){noteModelCommit({ModelCommitState::saved});return {ModelCommitState::saved};}
} somfy;
bool radioInstallPaused(){return somfy.transceiver.restorePaused;}
struct ConfigSettings { bool settingsUsable()const{return Security.loadTrusted();} bool hasOtaSlot=true;struct { bool trusted=true;bool loadTrusted()const{return trusted;} } Security; } settings; // Qualified boot fixture.
#define configsettings_h
static std::function<void()> duringAuthentication;
struct Web {
  bool uploadSuccess = false;
  bool isAuthenticated(AsyncWebServerRequest *request){if(duringAuthentication)duringAuthentication();return request->allowed&&settings.Security.trusted;}
} webServer;
struct MqttConnection {
  unsigned stops = 0;
  void end() { ++stops; }
} mqtt;
FakeUpdate Update;

static SettingsRevisions restoreSecurityHistory;
uint64_t securityConfigurationRevision(){return restoreSecurityHistory.get(settings_section_t::security);}
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
  static RestoreApplyResult restore(SomfyShadeController*,const char*,restore_options_t &opts,const GitUpdateOwnerToken &owner,const RadioInstallToken &radio) {
    assert(gitUpdate.transferRestoreClaimed(owner,manualClockMs)&&restoreExecutionHolds(owner,radio));
    RestoreApplyResult result;
    if(somfy.transceiver.uncertain)return result;
    restoreSecurityHistory.invalidateAll(); // Actual restore invalidates history after consuming its grant.
    ++restores;result.complete=restoreValid;result.refusal=restoreValid?RestoreRefusal::none:RestoreRefusal::invalid;
    result.modelApplied=opts.shades||opts.repeaters;result.modelFile={ModelCommitState::saved};
    if(opts.settings||opts.network||opts.mqtt||opts.transceiver){
      result.settings.state=RestoreSettingsState::saved;
      if(opts.settings)result.settings.requestedNamespaces|=settingsNamespaceBit(SettingsNamespace::cfg)|settingsNamespaceBit(SettingsNamespace::ntp);
      if(opts.network)result.settings.requestedNamespaces|=settingsNamespaceBit(SettingsNamespace::cfg)|settingsNamespaceBit(SettingsNamespace::ip);
      if(opts.mqtt)result.settings.requestedNamespaces|=settingsNamespaceBit(SettingsNamespace::mqtt);
      if(opts.transceiver)result.settings.requestedNamespaces|=settingsNamespaceBit(SettingsNamespace::radio);
      result.settings.verifiedNamespaces=result.settings.appliedNamespaces=result.settings.requestedNamespaces;
    }
    return result;
  }
};
struct RecordingRequests {shade_request_result_t run(const shade_request_t&){return shade_request_result_t::done;}} shadeRequests;
#define LOCAL_QUEUE_EXTERNAL_REBOOT
#define LOCAL_QUEUE_EXTERNAL_CONFIG_FILE
#define LOCAL_QUEUE_REAL_UPDATE_OWNER
#define LOCAL_QUEUE_REAL_RESTORE_EXECUTION
#include "../ui-adapter/LoopHarness.h"
bool asyncHasParam(AsyncWebServerRequest *request,const char *name){return std::string(name)=="data"?request->hasRestoreOptions:request->params.count(name);}
String asyncParam(AsyncWebServerRequest *request,const char *name){return String(std::string(name)=="data"?request->restoreOptions:request->params[name]);}
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
void completeRestoreModel(const RestoreApplyResult &result){assert(result.modelApplied);}
uint32_t restoreTestRandom(){return 42;}
#include "../../src/RestoreExecution.cpp"
#include "ApplicationFilesystem.h"
#include "entryPoints/PreparedApplicationUpload.h"
// This harness covers legacy firmware/restore and refusal before preparation.
// The physical/application HTTP journey uses the real actor/receiver separately.
ApplicationFilesystemSnapshot applicationFilesystemStatus(){return {};}
bool noteApplicationPreparation(const GitUpdateOwnerToken &,const ManualApplicationSource &){return false;}
void refuseUnpublishedApplication(const GitUpdateOwnerToken &){}
bool requestApplicationFilesystemRecovery(const GitUpdateOwnerToken &,const ManualApplicationSource &){return false;}
void receivePreparedApplication(AsyncWebServerRequest *,const GitUpdateOwnerToken &,const ManualApplicationSource &,size_t,uint8_t *,size_t,bool){assert(false);}
bool finishPreparedApplication(AsyncWebServerRequest *,const GitUpdateOwnerToken &,const ManualApplicationSource &){return false;}
// Physical transitions are exercised against the real owner in application-filesystem.
void serviceApplicationFilesystem(){}
#include "../../src/entryPoints/ManualUploadSession.cpp"
#include "../../src/entryPoints/ApplicationUpload.cpp"
#include "../../src/entryPoints/FirmwareUpload.cpp"
#include "../../src/entryPoints/RestoreUpload.cpp"
static void reset() {
  cancelRestart();restartLoop();
  manualClockMs+=60000;manualUploadLoop();loopQueue.drain();
  settings.hasOtaSlot=true;settings.Security.trusted=true;
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
  assert(beginRestoreExecution());
  AsyncWebServer server;
  registerApplicationUploadRoute(server);
  uint8_t byte = 42;loopQueue.begin();
  reset();
  settings.Security.trusted=false;restartLoop();
  AsyncWebServer untrustedFirmware;registerFirmwareUploadRoute(untrustedFirmware);
  AsyncWebServerRequest untrusted;
  untrustedFirmware.upload(&untrusted,String("blocked.bin"),0,&byte,1,true);untrustedFirmware.complete(&untrusted);
  assert(untrusted.status==503 && !Update.begins && !Update.writes && !Update.ends && !Update.aborts);
  assert(!somfy.transceiver.stops && !mqtt.stops && !gitUpdate.busy() && restarts.empty());
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
  std::vector<uint8_t> image(1024,42);
  AsyncWebServer firmware;registerFirmwareUploadRoute(firmware);
  AsyncWebServerRequest staged;
  firmware.upload(&staged,String("firmware.bin"),0,image.data(),512,false);
  assert(stagedAllocations==1 && !Update.begins && !somfy.transceiver.stops && !mqtt.stops);
  firmware.upload(&staged,String("firmware.bin"),512,image.data()+512,512,true);
  firmware.complete(&staged);
  assert(staged.status==202 && restoreMarker.type==loop_item_type_t::firmware_install);
  assert(staged.body.find("received")!=std::string::npos && !Update.ends && !restartPending());
  staged.disconnected();assert(gitUpdate.busy()&&stagedAllocations==1);
  loopQueue.drain(); // This host build has no installer worker: refuse, free, never write.
  assert(!stagedAllocations&&!gitUpdate.busy()&&!Update.begins&&!Update.aborts);
  auto noEffects=[](){assert(!Update.begins&&!Update.writes&&!Update.ends&&!Update.aborts&&!somfy.transceiver.stops&&!mqtt.stops&&!restartPending());};
  auto receive=[&](AsyncWebServerRequest &request){firmware.upload(&request,String("firmware.bin"),0,image.data(),image.size(),true);};
  AsyncWebServerRequest partial;
  firmware.upload(&partial,String("firmware.bin"),0,image.data(),512,false);
  partial.disconnected();manualUploadLoop();assert(!stagedAllocations&&!gitUpdate.busy());noEffects();
  AsyncWebServerRequest timeout;
  firmware.upload(&timeout,String("firmware.bin"),0,image.data(),512,false);
  manualClockMs+=30000;manualUploadLoop();assert(!stagedAllocations&&!gitUpdate.busy());
  AsyncWebServerRequest replacement;receive(replacement);
  const auto replacementOwner=manualUploadState(&replacement)->owner;
  firmware.upload(&timeout,String("firmware.bin"),512,image.data(),512,true);
  timeout.disconnected();manualUploadLoop();assert(stagedAllocations==1&&gitUpdate.ownsExternal(replacementOwner));
  replacement.disconnected();manualUploadLoop();assert(!stagedAllocations&&!gitUpdate.busy());noEffects();
  AsyncWebServerRequest oversized;
  firmware.upload(&oversized,String("firmware.bin"),0,image.data(),GitUpdatePolicy::APP_BYTES+1ULL,true);
  assert(!stagedAllocations&&!gitUpdate.busy());noEffects();
  AsyncWebServerRequest noMemory;failStagedAllocation=true;receive(noMemory);failStagedAllocation=false;
  assert(!stagedAllocations&&!gitUpdate.busy());noEffects();
  AsyncWebServerRequest unavailable;receive(unavailable);reserveReady=false;firmware.complete(&unavailable);reserveReady=true;
  assert(unavailable.status==503&&!stagedAllocations&&!gitUpdate.busy());noEffects();
  AsyncWebServerRequest tooSmall;
  firmware.upload(&tooSmall,String("firmware.bin"),0,image.data(),1,true);
  assert(!stagedAllocations&&!gitUpdate.busy());noEffects();
  AsyncWebServerRequest wrongOffset;
  firmware.upload(&wrongOffset,String("firmware.bin"),0,image.data(),512,false);
  firmware.upload(&wrongOffset,String("firmware.bin"),513,image.data(),512,true);
  manualUploadLoop();assert(!stagedAllocations&&!gitUpdate.busy());noEffects();
  AsyncWebServerRequest twoFiles;receive(twoFiles);receive(twoFiles);firmware.complete(&twoFiles);
  manualUploadLoop();assert(twoFiles.status==409&&!stagedAllocations&&!gitUpdate.busy());noEffects();
  AsyncWebServerRequest credentialChanged;receive(credentialChanged);
  restoreSecurityHistory.invalidateAll();firmware.complete(&credentialChanged);
  assert(credentialChanged.status==503&&!stagedAllocations&&!gitUpdate.busy());noEffects();
  AsyncWebServerRequest authChanged;receive(authChanged);authChanged.allowed=false;firmware.complete(&authChanged);
  assert(authChanged.status==503&&!stagedAllocations&&!gitUpdate.busy());noEffects();
  AsyncWebServerRequest queueFull;receive(queueFull);restoreQueue=false;firmware.complete(&queueFull);restoreQueue=true;
  assert(queueFull.status==503&&queueFull.responses==1&&!stagedAllocations&&!gitUpdate.busy());noEffects();
  AsyncWebServerRequest lifetime;
  firmware.upload(&lifetime,String("firmware.bin"),0,image.data(),128,false);
  for(size_t i=1;i<9;++i){manualClockMs+=20000;firmware.upload(&lifetime,String("firmware.bin"),i*128,image.data(),128,false);}
  manualClockMs+=20000;firmware.upload(&lifetime,String("firmware.bin"),1152,image.data(),128,true);
  manualUploadLoop();assert(!stagedAllocations&&!gitUpdate.busy());noEffects();
  AsyncWebServerRequest queuedExpired;receive(queuedExpired);firmware.complete(&queuedExpired);
  manualClockMs+=30000;manualUploadLoop();loopQueue.drain();assert(!stagedAllocations&&!gitUpdate.busy());noEffects();
  AsyncWebServerRequest statusDenied;statusDenied.allowed=false;firmware.statusGet(&statusDenied);assert(statusDenied.status==401);
  for(const char *operation:{"0","01","18446744073709551616","x","1.0","-1",""}) {
    AsyncWebServerRequest invalid;invalid.params["operationGeneration"]=operation;
    firmware.statusGet(&invalid);assert(invalid.status==400);
  }
  AsyncWebServerRequest staleStatus;staleStatus.params["operationGeneration"]="9007199254740993";
  firmware.statusGet(&staleStatus);assert(staleStatus.status==409);noEffects();
  overrideFirmwareStatus=true;firmwareStatus.manual=true;firmwareStatus.operation=9007199254740993ULL;
  firmwareStatus.phase=GitUpdatePhase::written;firmwareStatus.busy=true;
  AsyncWebServerRequest exactStatus;exactStatus.params=staleStatus.params;firmware.statusGet(&exactStatus);
  JsonDocument statusJson;assert(!deserializeJson(statusJson,exactStatus.body)&&exactStatus.status==200);
  assert(statusJson["operationGeneration"].as<std::string>()=="9007199254740993"&&statusJson["state"].as<std::string>()=="written");
  assert(!statusJson["restartScheduled"].as<bool>()&&statusJson["source"].as<std::string>()=="manual-file");
  assert(exactStatus.body.find("owner")==std::string::npos&&exactStatus.body.find("security")==std::string::npos);
  firmwareStatus.operation++;
  AsyncWebServerRequest replacedStatus;replacedStatus.params=staleStatus.params;firmware.statusGet(&replacedStatus);assert(replacedStatus.status==409);
  overrideFirmwareStatus=false;noEffects();
  std::cout << "Bundled upload stages complete file and hands it to loop without SDK/shutdown effects.\n";return 0;
#endif
  AsyncWebServerRequest raw;
  server.upload(&raw,String("unprepared.littlefs.bin"),0,&byte,1,true);server.complete(&raw);
  assert(raw.status==400&&!Update.begins&&!Update.writes&&!Update.aborts&&!gitUpdate.busy()&&!restartPending());
  AsyncWebServerRequest unauthenticated;unauthenticated.allowed=false;
  server.upload(&unauthenticated,String("blocked.bin"),0,&byte,1,true);server.complete(&unauthenticated);
  assert(unauthenticated.status==401&&!Update.begins);
  reset();
  AsyncWebServer firmwareServer;registerFirmwareUploadRoute(firmwareServer);
  settings.hasOtaSlot=false;AsyncWebServerRequest unsupported;
  firmwareServer.upload(&unsupported,String("x.bin"),0,&byte,1,true);firmwareServer.complete(&unsupported);
  assert(unsupported.status==400 && !Update.begins && !restartPending());
  reset();
  AsyncWebServerRequest appActive,firmwareOverlap;
  GitUpdateOwnerToken appOwner;assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,appOwner));assert(gitUpdate.beginTransfer(appOwner,ManualUpdateKind::application,manualClockMs));
  firmwareServer.upload(&firmwareOverlap,String("x.bin"),0,&byte,1,true);firmwareServer.complete(&firmwareOverlap);
  assert(Update.begins==0 && firmwareOverlap.status==503 && !restartPending());
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
  GitUpdateOwnerToken nextFirmware;assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,nextFirmware));
  assert(gitUpdate.beginTransfer(nextFirmware,ManualUpdateKind::firmware,manualClockMs)&&gitUpdate.claimTransfer(nextFirmware,manualClockMs));
  assert(gitUpdate.transferResource(nextFirmware,true)&&gitUpdate.finishTransfer(nextFirmware,manualClockMs,true));Update.running=true;
  const auto beforeLate=Update.writes;
  firmwareServer.upload(&firmwareOld,String("x.bin"),1,&byte,1,true);firmwareOld.disconnected();
  assert(Update.writes==beforeLate && Update.aborts==1 && !restartPending());
  reset();
  AsyncWebServer restoreServer;registerRestoreUploadRoute(restoreServer);
  AsyncWebServerRequest busyImage,busyRestore;
  GitUpdateOwnerToken busyOwner;assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,busyOwner));assert(gitUpdate.beginTransfer(busyOwner,ManualUpdateKind::firmware,manualClockMs));
  restoreServer.upload(&busyRestore,String("x.backup"),0,&byte,1,true);
  assert(LittleFS.opens==0 && LittleFS.writes==0);
  reset();
  AsyncWebServerRequest restoreSuccess;
  restoreServer.upload(&restoreSuccess,String("x.backup"),0,&byte,1,true);restoreServer.complete(&restoreSuccess);
  assert(restoreSuccess.status==202 && restoreMarker.externalOwner.generation && restoreMarker.externalOwner.kind==GitUpdateExternalOwner::restore);
  const auto restoreSource=restoreMarker.externalOwner;
  const auto status=restoreExecutionStatus();
  assert(restoreSuccess.body.find("\"bootSession\":\""+std::to_string(status.bootSession)+"\"")!=std::string::npos);
  assert(restoreSuccess.body.find("\"generation\":\""+std::to_string(restoreSource.generation)+"\"")!=std::string::npos);
  assert(restoreServer.statusGet);
  AsyncWebServerRequest query;query.params={{"bootSession",std::to_string(status.bootSession)},{"generation",std::to_string(status.generation)}};
  query.allowed=false;restoreServer.statusGet(&query);assert(query.status==401);
  query.allowed=true;query.params["generation"]="01";restoreServer.statusGet(&query);assert(query.status==400);
  query.params["generation"]="18446744073709551616";restoreServer.statusGet(&query);assert(query.status==400);
  query.params["generation"]=std::to_string(status.generation);query.params["bootSession"]="1";
  restoreServer.statusGet(&query);assert(query.status==409);
  query.params["bootSession"]=std::to_string(status.bootSession);restoreServer.statusGet(&query);
  assert(query.status==200&&query.body.find("\"phase\":\"queued\"")!=std::string::npos);

  restoreSuccess.disconnected();assert(gitUpdate.ownsExternal(restoreSource));
  loopQueue.drain();assert(restores==1 && restartPending() && !gitUpdate.ownsExternal(restoreSource));
  assert(!LittleFS.files.count("/shades.tmp") && gitUpdate.busy());
  restoreServer.statusGet(&query);assert(query.status==200&&query.body.find("\"phase\":\"applied\"")!=std::string::npos&&query.body.find("\"restartScheduled\":true")!=std::string::npos);
  for(unsigned revoke=0;revoke<3;++revoke){
    reset();AsyncWebServerRequest revokedQueued;
    restoreServer.upload(&revokedQueued,String("x.backup"),0,&byte,1,true);restoreServer.complete(&revokedQueued);
    assert(revokedQueued.status==202);
    if(revoke==0)settings.Security.trusted=false;
    else for(unsigned save=0;save<revoke;++save)restoreSecurityHistory.completed(settings_section_t::security);
    loopQueue.drain();assert(restores==0 && !restartPending() && !gitUpdate.busy() && !LittleFS.files.count("/shades.tmp"));
    assert(restoreExecutionStatus().phase==RestoreExecutionPhase::refused);
  }
  reset();AsyncWebServerRequest revokedBeforeCompletion;
  restoreServer.upload(&revokedBeforeCompletion,String("x.backup"),0,&byte,1,true);
  restoreSecurityHistory.completed(settings_section_t::security);
  restoreServer.complete(&revokedBeforeCompletion);
  assert(revokedBeforeCompletion.status==409&&!loopQueue.pending()&&!gitUpdate.busy()&&!LittleFS.files.count("/shades.tmp"));
  reset();AsyncWebServerRequest revokedToken;
  restoreServer.upload(&revokedToken,String("x.backup"),0,&byte,1,true);revokedToken.allowed=false;
  restoreServer.complete(&revokedToken);assert(revokedToken.status==409&&!loopQueue.pending()&&!gitUpdate.busy());
  reset();AsyncWebServerRequest changingAdmission;bool changed=false;
  duringAuthentication=[&]{if(!changed){changed=true;restoreSecurityHistory.completed(settings_section_t::security);}};
  restoreServer.upload(&changingAdmission,String("x.backup"),0,&byte,1,true);duringAuthentication={};
  restoreServer.complete(&changingAdmission);
  assert(changed&&changingAdmission.status==409&&!LittleFS.opens&&!loopQueue.pending()&&!gitUpdate.busy());
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
  assert(restoreExecutionStatus().phase==RestoreExecutionPhase::refused);
  for(const bool disconnect:{false,true}){
    reset();AsyncWebServerRequest gap;
    restoreServer.upload(&gap,String("x.backup"),0,&byte,1,true);bool fired=false;
    duringClock=[&]{const auto status=restoreExecutionStatus();if(!fired&&status.phase==RestoreExecutionPhase::queued&&status.generation==manualUploadState(&gap)->owner.generation){fired=true;if(disconnect)gap.disconnected();else manualClockMs+=30001;}};
    restoreServer.complete(&gap);duringClock={};
    assert(fired);
    if(disconnect){
      // The existing lifecycle intentionally retains a queued source across
      // HTTP disconnect; its exact FIFO marker still executes only once.
      assert(gap.status==202&&loopQueue.pending()==1);
      loopQueue.drain();assert(restores==1&&restoreExecutionStatus().phase==RestoreExecutionPhase::applied);
    }else{
      assert(gap.status==409&&!loopQueue.pending()&&restoreExecutionStatus().phase==RestoreExecutionPhase::refused);
      manualUploadLoop();assert(!gitUpdate.busy());
    }
  }
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

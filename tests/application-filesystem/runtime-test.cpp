#include <cassert>
#include <cstring>
#include <string>
#include "ApplicationFilesystem.h"
#include "MountedFilesystemGate.h"
#include "ModelLoadTrust.h"
#include "ModelCommitService.h"
#include "LoopQueue.h"
#include "SafeRestart.h"
#include "RadioUpdateActivity.h"
#include "esp_timer.h"
#include "esp_partition.h"
#include "LittleFS.h"
#include "Update.h"
#define SOMFY_H
#define configsettings_h
struct ConfigSettings{struct{bool trusted=true;bool loadTrusted()const{return trusted;}}Security;}settings;
static uint64_t revision=9;uint64_t securityConfigurationRevision(){return revision;}
static unsigned restartCalls=0;static bool denyRestart=false;static GitUpdateOwnerToken restartOwner;
static bool blocked=false;bool restartTransferBlocked(){return blocked;}bool restartPending(){return false;}
static bool queued=false;static bool sealed=false;
bool LoopQueue::sealForRestart(){if(queued||sealed)return false;sealed=true;return true;}
void LoopQueue::reopenAfterRefusedRestart(){assert(sealed);sealed=false;}
LoopQueue loopQueue;
static unsigned saves=0;static bool saveFails=false;static int currentCounter=31,savedCounter=0;
ModelCommitReport applyModelCommit(const proto_reply_to_t &,ModelCommitMode){++saves;savedCounter=currentCounter;return {{saveFails?ModelCommitState::failed:ModelCommitState::saved,ModelCommitStage::none}};}
struct SomfyShadeController {
 ModelLoadTrust modelLoad{ModelLoadStatus::loaded};bool isDirty=false;
 struct Shade{bool idle=true;uint8_t getShadeId()const{return 7;}bool idleForAdministration()const{return idle;}}shades[1];
 struct Group{uint8_t getGroupId()const{return 255;}uint8_t linkedShades[32]{};int8_t direction=0;somfy_frame_t lastFrame;}groups[1];
 Shade *getShadeById(uint8_t){return &shades[0];}
 struct Radio{bool paused=false,resumeBusy=false;unsigned resumes=0;
  bool pauseForRestore(RadioInstallToken &token){assert(sealed);paused=true;token.generation=1;return true;}
  RadioInstallResume resumeAfterInstall(const RadioInstallToken &token){assert(token.generation==1);++resumes;if(resumeBusy)return RadioInstallResume::busy;paused=false;return RadioInstallResume::resumed;}
 }transceiver;
}somfy;
FakeLittleFS LittleFS;FakeUpdate Update;
RestartResult requestOwnedRestart(const GitUpdateOwnerToken &owner,uint32_t){
 ++restartCalls;assert(LittleFS.mounted&&!somfy.transceiver.paused&&savedCounter==currentCounter);
 if(denyRestart)return {false,RestartReason::activeMovement};
 const bool admitted=gitUpdate.acquireRestart(restartOwner,uint64_t(stubEspTimerMicros)/1000,owner);
 return {admitted,admitted?RestartReason::none:RestartReason::stale};
}
esp_partition_t mountedPartition{1,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,0x310000,4096,"spiffs"},updatePartition=mountedPartition;
const esp_partition_t *esp_partition_find_first(int,int,const char *label){return label?&mountedPartition:&updatePartition;}
bool esp_littlefs_mounted(const char *label){assert(!strcmp(label,"spiffs"));return LittleFS.mounted;}
#include "../../src/ApplicationFilesystem.cpp"
#include "ESPAsyncWebServer.h"
#define webserver_h
struct Web{bool isAuthenticated(AsyncWebServerRequest *request){return request->allowed&&settings.Security.trusted;}}webServer;
#include "entryPoints/ManualUploadSession.h"
ManualUploadState *manualUploadState(AsyncWebServerRequest *request){return request->getAttribute("manualState",false)?static_cast<ManualUploadState *>(request->_tempObject):nullptr;}
#include "../../src/entryPoints/PreparedApplicationUpload.cpp"
const char _encoding_json[]="application/json";
String asyncParam(AsyncWebServerRequest *request,const char *name){return String(request->params[name]);}
static bool metadataQueue=true;static GitUpdateOwnerToken queuedOwner;
bool submitModelCommit(const proto_reply_to_t &,ModelCommitMode mode,const GitUpdateOwnerToken &owner){assert(mode==ModelCommitMode::applicationPreparation);queuedOwner=owner;return metadataQueue;}
static bool entropyFails=false;uint32_t applicationTestRandom(){return entropyFails?0:123;}
static void resetTaskWdt(){}
#define TASK_WDT_H
#include "../../src/entryPoints/ApplicationUpload.cpp"

int main(int argc,char **argv){
 assert(argc==2);const std::string test=argv[1];const ManualApplicationSource source{123,revision,4096};GitUpdateOwnerToken owner;
 if(test.rfind("http",0)==0){
  AsyncWebServer server;registerApplicationUploadRoute(server);AsyncWebServerRequest prepare;
  prepare.params["bytes"]="4096";
  if(test=="httpAuth")prepare.allowed=false;
  if(test=="httpBytes")prepare.params["bytes"]="04096";
  if(test=="httpEntropy")entropyFails=true;
  if(test=="httpQueue")metadataQueue=false;
  server.metadata.at("/prepareApplication:2")(&prepare);
  assert(!saves&&!LittleFS.ends&&!Update.begins&&!prepare.disconnected);
  if(test=="httpAuth"||test=="httpBytes"||test=="httpEntropy"){assert(prepare.status>=400&&!gitUpdate.busy());return 0;}
  if(test=="httpQueue"){
   assert(prepare.status==503&&applicationFilesystemStatus().phase==ApplicationFilesystemPhase::refused);
   const auto cleanup=gitUpdate.claimTransferCleanup(0);assert(cleanup.token.generation);assert(gitUpdate.closeTransfer(cleanup.token)&&gitUpdate.releaseExternal(cleanup.token));return 0;
  }
  assert(prepare.status==202&&queuedOwner.generation);const auto snapshot=applicationFilesystemStatus();
  AsyncWebServerRequest query;query.params={{"nonce",std::to_string(snapshot.source.nonce)},{"generation",std::to_string(snapshot.owner.generation)}};
  query.allowed=false;server.metadata.at("/applicationUpdateStatus:1")(&query);assert(query.status==401);
  query.allowed=true;query.params["nonce"]="01";server.metadata.at("/applicationUpdateStatus:1")(&query);assert(query.status==400);
  query.params["nonce"]=std::to_string(snapshot.source.nonce);server.metadata.at("/applicationUpdateStatus:1")(&query);assert(query.status==200);
  runApplicationPreparation(snapshot.owner);serviceApplicationFilesystem();assert(!LittleFS.mounted);
  if(test=="httpCancel"){
   query.params["action"]="cancel";server.metadata.at("/applicationUpdate:2")(&query);assert(query.status==202);serviceApplicationFilesystem();assert(!gitUpdate.busy()&&LittleFS.mounted);return 0;
  }
  uint8_t bytes[4096]{};AsyncWebServerRequest file;file.params=query.params;
  if(test=="httpReauth"){
   server.upload(&file,String("web.littlefs.bin"),0,bytes,2048,false);file.allowed=false;
   server.upload(&file,String("web.littlefs.bin"),2048,bytes,2048,true);server.complete(&file);
   assert(file.status==401);currentCounter=71;serviceApplicationFilesystem();
   assert(!gitUpdate.busy()&&LittleFS.mounted&&!restartCalls&&Update.aborts==1&&savedCounter==71);return 0;
  }
  server.upload(&file,String("web.littlefs.bin"),0,bytes,4096,true);assert(!restartCalls);
  server.complete(&file);assert(file.status==202&&!restartCalls);assert(applicationFilesystemStatus().phase==ApplicationFilesystemPhase::recovering);currentCounter=71;serviceApplicationFilesystem();
  assert(applicationFilesystemStatus().uploaded&&restartCalls==1&&savedCounter==71);assert(gitUpdate.releaseExternal(restartOwner));return 0;
 }
 assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,owner));assert(gitUpdate.beginTransfer(owner,ManualUpdateKind::application,0));
 assert(gitUpdate.prepareApplication(owner,source,0));assert(noteApplicationPreparation(owner,source));
 auto reader=mountedFilesystemGate().enter();assert(reader);
 if(test=="layout")updatePartition.address+=4096;
 if(test=="littlefs")mountedPartition.subtype=updatePartition.subtype=ESP_PARTITION_SUBTYPE_DATA_LITTLEFS;
 if(test=="size")mountedPartition.size=updatePartition.size=8192;
 if(test=="save")saveFails=true;
 if(test=="busy")queued=true;
 if(test=="moving")somfy.shades[0].idle=false;
 if(test=="security")++revision;
 runApplicationPreparation(owner);
 if(test=="layout"||test=="size"||test=="save"||test=="busy"||test=="moving"||test=="security"){
  assert(applicationFilesystemStatus().phase==ApplicationFilesystemPhase::refused);
  assert(!gitUpdate.busy()&&LittleFS.mounted&&!LittleFS.ends&&!somfy.transceiver.paused);return 0;
 }
 assert(applicationFilesystemStatus().phase==ApplicationFilesystemPhase::draining);
 assert(!sealed&&somfy.transceiver.paused&&LittleFS.mounted&&!mountedFilesystemGate().enter());
 serviceApplicationFilesystem();assert(!LittleFS.ends);reader.close();
 if(test=="unmount")LittleFS.unmountFails=true;
 if(test=="expired")stubEspTimerMicros=30000000;
 serviceApplicationFilesystem();
 if(test=="unmount"||test=="expired"){
  assert(LittleFS.mounted&&!gitUpdate.busy()&&!somfy.transceiver.paused);
  assert(applicationFilesystemStatus().phase==ApplicationFilesystemPhase::refused);return 0;
 }
 assert(applicationFilesystemStatus().phase==ApplicationFilesystemPhase::ready);
 assert(!LittleFS.mounted&&gitUpdate.busy());int command=0;
 assert(applicationFilesystemReady(owner,source,command)&&command==(test=="littlefs"?U_LITTLEFS:U_SPIFFS));
 // Original metadata request disconnect must not cancel the prepared source.
 assert(!gitUpdate.retireTransfer(owner));
 if(test.rfind("receive",0)==0){
  AsyncWebServerRequest request;uint8_t bytes[4096]{};
  if(test=="receiveAuth")request.allowed=false;
  if(test=="receiveBegin"||test=="receiveForeign")Update.beginOk=false;
  if(test=="receiveForeign")Update.running=true;
  if(test=="receiveShort")Update.shortWrite=true;
  if(test=="receiveEnd")Update.endOk=false;
  if(test=="receiveChange")Update.onWrite=[] {++revision;};
  receivePreparedApplication(&request,owner,source,0,bytes,2048,false);
  if(test=="receiveOverlap"){
   AsyncWebServerRequest overlap;receivePreparedApplication(&overlap,owner,source,0,bytes,1,true);
   assert(!finishPreparedApplication(&overlap,owner,source));assert(gitUpdate.transferContinuing(owner,0));
  }
  if(test=="receiveDisconnect"&&request.disconnected)request.disconnected();
  receivePreparedApplication(&request,owner,source,test=="receiveIndex"?2049:2048,bytes,test=="receiveLong"?2049:2048,true);
  assert(!applicationFilesystemStatus().uploaded&&!restartCalls); // file-final is not HTTP completion
  if(test=="receiveSecond")receivePreparedApplication(&request,owner,source,0,bytes,1,true);
  if(test=="receiveExpired")stubEspTimerMicros=30000000;
  const bool completed=finishPreparedApplication(&request,owner,source);
  const bool success=test=="receiveGood"||test=="receiveOverlap";assert(completed==success);
  if(test=="receiveAuth"){assert(!Update.begins);assert(gitUpdate.cancelApplicationPreparation(owner,source));}
  currentCounter=61;serviceApplicationFilesystem();
  assert(LittleFS.mounted&&!somfy.transceiver.paused&&savedCounter==61);
  assert(restartCalls==(success?1u:0u));if(success)assert(gitUpdate.releaseExternal(restartOwner));
  if(test=="receiveForeign")assert(Update.running&&!Update.aborts);
  assert(!gitUpdate.busy());return 0;
 }
 if(test=="completed"||test=="restartDenied"||test=="expiredComplete"){
  assert(gitUpdate.claimPreparedApplication(owner,source,0));assert(gitUpdate.transferResource(owner,true));assert(gitUpdate.transferResource(owner,false));
  assert(completePreparedApplication(owner,source));assert(gitUpdate.finishTransfer(owner,0,true));
  denyRestart=test=="restartDenied";if(test=="expiredComplete")stubEspTimerMicros=30000000;
  currentCounter=57;serviceApplicationFilesystem();
  const auto result=applicationFilesystemStatus();assert(result.uploaded&&result.phase==ApplicationFilesystemPhase::completed);
  assert(result.restartScheduled==(test=="completed"));assert(restartCalls==(test=="expiredComplete"?0u:1u));
  if(test=="completed")assert(gitUpdate.releaseExternal(restartOwner));
  assert(!gitUpdate.busy()&&savedCounter==57);serviceApplicationFilesystem();assert(restartCalls<=1);return 0;
 }
 if(test=="mount")LittleFS.mountFails=true;
 if(test=="radio")somfy.transceiver.resumeBusy=true;
 if(test=="recoverySave")saveFails=true;
 if(test=="foreignSdk")Update.running=true;
 if(test=="sdk"||test=="abortBusy"){
  assert(gitUpdate.claimPreparedApplication(owner,source,0));assert(gitUpdate.transferResource(owner,true));Update.running=true;
  Update.abortFails=test=="abortBusy";assert(gitUpdate.retireTransfer(owner));assert(gitUpdate.finishTransfer(owner,0,false));
 }else if(test=="revokedReady")++revision;
 else if(test=="expiredReady")stubEspTimerMicros=30000000;
 else assert(gitUpdate.cancelApplicationPreparation(owner,source));
 currentCounter=45;
 serviceApplicationFilesystem();
 if(test=="abortBusy"){
  assert(gitUpdate.busy()&&!LittleFS.mounted&&Update.running);Update.abortFails=false;serviceApplicationFilesystem();
 }
 if(test=="sdk"||test=="abortBusy")assert(!Update.running&&Update.aborts);
 if(test=="foreignSdk")assert(Update.running&&!Update.aborts);
 if(test=="mount"||test=="recoverySave"){
  assert(applicationFilesystemStatus().phase==ApplicationFilesystemPhase::attention&&gitUpdate.busy());
  assert(!somfy.transceiver.paused);
  auto wrong=source;++wrong.nonce;assert(!requestApplicationFilesystemRecovery(owner,wrong));
  assert(requestApplicationFilesystemRecovery(owner,source));LittleFS.mountFails=false;saveFails=false;
  currentCounter=67;serviceApplicationFilesystem();
 }
 if(test=="radio"){
  assert(gitUpdate.busy()&&somfy.transceiver.paused);somfy.transceiver.resumeBusy=false;serviceApplicationFilesystem();
 }
 assert(LittleFS.mounted&&!somfy.transceiver.paused&&!gitUpdate.busy()&&saves==(test=="recoverySave"?3u:2u)&&savedCounter==((test=="mount"||test=="recoverySave")?67:45));
 assert(applicationFilesystemStatus().phase==ApplicationFilesystemPhase::refused&&mountedFilesystemGate().enter());
}

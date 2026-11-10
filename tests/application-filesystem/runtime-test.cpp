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
esp_partition_t mountedPartition{1,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,0x310000,4096,"spiffs"},updatePartition=mountedPartition;
const esp_partition_t *esp_partition_find_first(int,int,const char *label){return label?&mountedPartition:&updatePartition;}
bool esp_littlefs_mounted(const char *label){assert(!strcmp(label,"spiffs"));return LittleFS.mounted;}
#include "../../src/ApplicationFilesystem.cpp"
int main(int argc,char **argv){
 assert(argc==2);const std::string test=argv[1];const ManualApplicationSource source{123,revision,4096};GitUpdateOwnerToken owner;
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
 if(test=="mount")LittleFS.mountFails=true;
 if(test=="radio")somfy.transceiver.resumeBusy=true;
 if(test=="recoverySave")saveFails=true;
 if(test=="foreignSdk")Update.running=true;
 if(test=="sdk"||test=="abortBusy"){
  assert(gitUpdate.claimPreparedApplication(owner,source,0));assert(gitUpdate.transferResource(owner,true));Update.running=true;
  Update.abortFails=test=="abortBusy";assert(gitUpdate.retireTransfer(owner));assert(gitUpdate.finishTransfer(owner,0,false));
 }else if(test=="expiredReady")stubEspTimerMicros=30000000;
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

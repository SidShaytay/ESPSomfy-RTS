// The real worker, controller adapter and SafeRestart run together. Only
// physical SDK/model-file/authorization observations are replaced.
#define arduino_stub_h
using byte=uint8_t;
#include "ModelLoadTrust.h"
#include "SettingsLoadState.h"
#include "SettingsDurability.h"
#include "SafeRestart.h"
#include "LoopQueue.h"
#include "LocalUiAuthorizationAdapter.h"
#include "GitUpdateBootHealth.h"
static bool bootPending=false;
GitUpdateBootStatus gitUpdateBootStatus(){return {bootPending,false};}
static std::string controllerScenario;
static uint64_t controllerOperation=0;
static uint64_t webSecurityRevision=7;
uint64_t securityConfigurationRevision(){return webSecurityRevision;}
static bool filesystemReady=true,queueHeld=false,queuePending=false,localPending=false;
static bool saveOkay=true,pauseOkay=true,resumeBusy=false;
static bool stallManualAdoption=false;
static unsigned modelSaves=0,shutdownCalls=0,resumeCalls=0;
static void (*onModelSave)()=nullptr;
static LocalUi::SecuritySnapshot localSecurity{LocalUi::AuthMode::pin,true,false,3,5,0,0};
namespace LocalUi {
SecuritySnapshot pollAuthorization(){CHECK(!Fake::criticalDepth);return localSecurity;}
bool localControlPending(){return localPending;}
}
bool modelFilesystemMounted(){return filesystemReady;}
LoopQueue loopQueue;
uint32_t LoopQueue::pending()const{return queuePending?1:0;}
bool LoopQueue::sealForRestart(){if(queueHeld || queuePending)return false;queueHeld=true;return true;}
bool LoopQueue::restartSealHeld()const{return queueHeld && !queuePending;}
void LoopQueue::reopenAfterRefusedRestart(){CHECK(!Fake::inFlash);queueHeld=false;}
#define configsettings_h
struct ConfigSettings {
  bool usable=true;
  struct {bool trusted=true;bool loadTrusted()const{return trusted;}}Security;
  bool settingsUsable()const{return usable && Security.trusted;}
} settings;
#define SOMFY_H
struct SomfyShadeController {
  ModelLoadTrust modelLoad{ModelLoadStatus::loaded};
  uint64_t configurationRevision=1;
  bool isDirty=true;
  struct Radio {
    struct {SettingsLoadStatus loadStatus{SettingsLoadState::loaded};}config;
    bool held=false,work=false,uncertain=false;
    bool counterEditUncertain()const{if(stallManualAdoption){Fake::now+=31000;stallManualAdoption=false;}return uncertain;}
    bool restartQuiescent()const{return !work;}
    bool sealForRestart(){if(held || work)return false;held=true;return true;}
    void reopenAfterRefusedRestart(){held=false;}
    bool updateActivity(RadioUpdateActivity &out)const{out.observed=out.ready=true;out.schedulerIdle=!work;out.txSealed=held;return true;}
    bool pauseForInstall(RadioInstallToken &token){if(!pauseOkay || held || work)return false;held=true;token.generation=7;++Fake::prepareCalls;return true;}
    bool installPauseHeld(const RadioInstallToken &token)const{return token.generation==7 && held && !work;}
    RadioInstallResume resumeAfterInstall(const RadioInstallToken &token){
      CHECK(!Fake::inFlash);++resumeCalls;if(resumeBusy)return RadioInstallResume::busy;
      if(!installPauseHeld(token))return RadioInstallResume::stale;
      held=false;return RadioInstallResume::resumed;
    }
  }transceiver;
  struct Shade {uint8_t id=255;bool idle=true;uint8_t getShadeId()const{return id;}bool idleForAdministration()const{return idle;}}shades[32];
  struct Group {uint8_t id=255;int8_t direction=0;somfy_frame_t lastFrame;uint8_t linkedShades[32]{};uint8_t getGroupId()const{return id;}}groups[16];
  Shade *getShadeById(uint8_t id){for(auto &shade:shades)if(id && id!=255 && shade.id==id)return &shade;return nullptr;}
  ModelCommitResult commit(){
    CHECK(!Fake::criticalDepth && queueHeld && transceiver.held && !Fake::inFlash);++modelSaves;
    if(onModelSave)onModelSave();
    ModelCommitResult result{saveOkay?ModelCommitState::saved:ModelCommitState::failed,ModelCommitStage::write};
    isDirty=!result.saved();noteModelCommit(result);return result;
  }
}somfy;
void performLegacyShutdown(){CHECK(queueHeld && somfy.transceiver.held && gitUpdate.busy() && Fake::otaActivations==1);++shutdownCalls;}
// Separate production translation units both use a private Guard. Avoid a
// fixture-only name collision when including them together for this journey.
#define Guard SafeRestartFixtureGuard
#include "../../src/SafeRestart.cpp"
#undef Guard
#include "../../src/GitUpdateController.cpp"
static void controllerPump(){loopGitUpdateController();try{worker(nullptr);}catch(const FakeTaskExit &){}loopGitUpdateController();restartLoop();}
int main(int argc,char **argv){
  CHECK(argc==4);controllerScenario=argv[1];Fake::autoLoop=false;fixtures();
  std::ifstream input(argv[2],std::ios::binary);std::string image{std::istreambuf_iterator<char>(input),{}};
  manifest["size"]=image.size();manifest["app_compat"]=argv[3];manifest["sha256"]=hash(image);
  release["assets"][0]["size"]=image.size();release["assets"][0]["digest"]="sha256:"+hash(image);
  const std::string url=std::string("https://github.com/")+GitUpdatePolicy::REPOSITORY+"/releases/download/"+tag+"/"+GitUpdatePolicy::FIRMWARE_ASSET;
  Fake::responses[url].body=image;publish();restartLoop();beginGitUpdateController();
  if(controllerScenario.compare(0,6,"manual")==0){
    GitUpdateOwnerToken owner;CHECK(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,owner));
    CHECK(gitUpdate.beginTransfer(owner,ManualUpdateKind::firmware,Fake::now));CHECK(gitUpdate.claimTransfer(owner,Fake::now));
    CHECK(gitUpdate.transferResource(owner,true) && gitUpdate.queueTransfer(owner));CHECK(gitUpdate.finishTransfer(owner,Fake::now,true));
    CHECK(!gitUpdate.retireTransfer(owner));if(controllerScenario!="manualNoClaim")CHECK(gitUpdate.claimTransfer(owner,Fake::now,true));
    uint64_t reserved=0;
    if(controllerScenario=="manualReservationFailure") {
      auto wrong=owner;++wrong.generation;
      CHECK(!gitUpdate.reserveManualInstall(wrong,reserved)&&!reserved);
      const auto generation=candidateGeneration;candidateGeneration=UINT64_MAX;
      CHECK(!gitUpdate.reserveManualInstall(owner,reserved)&&!reserved);candidateGeneration=generation;
      CHECK(gitUpdate.reserveManualInstall(owner,reserved));
      CHECK(gitUpdate.transferResource(owner,false)&&gitUpdate.closeTransfer(owner)&&gitUpdate.releaseExternal(owner));
      auto snapshot=gitUpdate.snapshot();
      CHECK(snapshot.operation==reserved&&snapshot.manual&&snapshot.phase==GitUpdatePhase::error&&!snapshot.busy&&!Fake::otaBegins);
      GitUpdateOwnerToken next;CHECK(gitUpdate.acquireExternal(GitUpdateExternalOwner::counter,next));
      CHECK(!gitUpdate.snapshot().manual&&!gitUpdate.snapshot().operation);CHECK(gitUpdate.releaseExternal(next));return 0;
    }
    if(controllerScenario=="manualReserved") {
      CHECK(gitUpdate.reserveManualInstall(owner,reserved));
      CHECK(reserved && reserved!=owner.generation && gitUpdate.snapshot().operation==reserved && gitUpdate.snapshot().manual);
      uint64_t duplicate=42;CHECK(!gitUpdate.reserveManualInstall(owner,duplicate)&&duplicate==42);
      CHECK(!gitUpdate.requestCancel(reserved)); // Receive/queued ownership is not a worker cancellation.
    }
    std::vector<uint8_t> localBytes(image.begin(),image.end());
    auto *bytes=static_cast<uint8_t*>(heap_caps_malloc(image.size(),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));CHECK(bytes);memcpy(bytes,image.data(),image.size());
    if(controllerScenario=="manualBadMemory"){heap_caps_free(bytes);bytes=localBytes.data();}
    if(controllerScenario=="manualInvalid")bytes[0]=0;
    if(controllerScenario=="manualLateAdoption")stallManualAdoption=true;
    if(controllerScenario=="manualNotifyFailure")Fake::notifyFail=true;
    uint64_t operation=123;const auto grant=controllerScenario=="manualWrongGrant"?webSecurityRevision+1:webSecurityRevision;
    const auto result=gitUpdate.requestManualInstall(owner,bytes,image.size(),grant,operation);
    if(controllerScenario=="manualWrongGrant" || controllerScenario=="manualBadMemory" || controllerScenario=="manualNoClaim" || controllerScenario=="manualLateAdoption"){
      CHECK(result!=GitUpdateRequest::accepted && bytes && operation==123 && !Fake::otaBegins);
      if(controllerScenario!="manualBadMemory")heap_caps_free(bytes);
      if(controllerScenario=="manualNoClaim")CHECK(gitUpdate.claimTransfer(owner,Fake::now,true));
      CHECK(gitUpdate.transferResource(owner,false));CHECK(gitUpdate.closeTransfer(owner));CHECK(gitUpdate.releaseExternal(owner));return 0;
    }
    CHECK(result==GitUpdateRequest::accepted && !bytes && operation && operation!=123);
    if(reserved)CHECK(operation==reserved);
    CHECK(!gitUpdate.retireTransfer(owner) && !gitUpdate.claimTransfer(owner,Fake::now));
    CHECK(gitUpdate.snapshot().manual);
    localSecurity.allowed=false;Fake::wallClock=0; // File installation works independently of LCD unlock and network time.
    if(controllerScenario=="manualOldAssets")Fake::fsCompatible=false;
    if(controllerScenario=="manualExpiredGrant")++webSecurityRevision;
    if(controllerScenario=="manualWriteFailure")Fake::failWrite=true;
    for(unsigned i=0;i<5;++i)controllerPump();
    CHECK(Fake::urls.empty() && !Fake::otaActivations && !shutdownCalls);
    if(controllerScenario=="manualValid" || controllerScenario=="manualOldAssets" || controllerScenario=="manualReserved"){
      CHECK(modelSaves==1 && restartPending());Fake::now+=500;controllerPump();CHECK(shutdownCalls==1 && Fake::otaActivations==1);return 0;
    }
    CHECK(!gitUpdate.busy() && !gitUpdateControllerPaused() && !Fake::activeAllocations);
    if(controllerScenario!="manualWriteFailure")CHECK(!modelSaves && !Fake::otaBegins);
    return 0;
  }
  uint64_t checkOperation=0;
  CHECK(requestLocalGitUpdateCheck(2,5,checkOperation)==GitUpdateRequest::unavailable);
  CHECK(requestLocalGitUpdateCheck(3,5,checkOperation)==GitUpdateRequest::accepted);controllerPump();
  const auto candidate=gitUpdate.snapshot().candidate;CHECK(candidate.generation && gitUpdate.snapshot().canInstall);
  uint64_t &operation=controllerOperation;
  CHECK(requestLocalGitUpdateInstall(candidate,3,4,operation)==GitUpdateRequest::unavailable && !operation);
  bootPending=true;CHECK(requestLocalGitUpdateInstall(candidate,3,5,operation)==GitUpdateRequest::unavailable && !operation);bootPending=false;
  CHECK(requestLocalGitUpdateInstall(candidate,3,5,operation)==GitUpdateRequest::accepted && operation!=candidate.generation);
  CHECK(!cancelLocalGitUpdate(3,5,checkOperation));
  if(controllerScenario=="moving"){somfy.shades[0].id=1;somfy.shades[0].idle=false;}
  if(controllerScenario=="queued")queuePending=true;
  if(controllerScenario=="localPending")localPending=true;
  if(controllerScenario=="radioWork")somfy.transceiver.work=true;
  if(controllerScenario=="pauseFailure")pauseOkay=false;
  if(controllerScenario=="saveFailure")saveOkay=false;
  if(controllerScenario=="sourceSave")onModelSave=[](){++localSecurity.configurationGeneration;};
  if(controllerScenario=="configSave")onModelSave=[](){++somfy.configurationRevision;};
  if(controllerScenario=="activationFailure")Fake::failActivate=true;
  if(controllerScenario=="cancelWrite" || controllerScenario=="cleanupBusy" || controllerScenario=="staleCleanup"){
    resumeBusy=controllerScenario=="cleanupBusy";
    Fake::onWrite=[](){CHECK(cancelLocalGitUpdate(3,5,controllerOperation));loopGitUpdateController();CHECK(gitUpdateControllerPaused() && !resumeCalls);if(controllerScenario=="staleCleanup")somfy.transceiver.held=false;};
  }
  if(controllerScenario=="sourceWrite")Fake::onWrite=[](){++localSecurity.configurationGeneration;loopGitUpdateController();CHECK(gitUpdateControllerPaused() && !resumeCalls);};
  if(controllerScenario=="inactivityStage" || controllerScenario=="deadlineStage"){
    Fake::onRead=[](){Fake::onRead=nullptr;Fake::now+=controllerScenario=="deadlineStage"?gitUpdateInstallDurationMs+1:120005;localSecurity.allowed=false;++localSecurity.generation;loopGitUpdateController();};
  }
  for(unsigned i=0;i<5;++i)controllerPump();
  CHECK(!shutdownCalls && !Fake::otaActivations);
  if(controllerScenario=="valid" || controllerScenario=="inactivityStage" || controllerScenario=="activationFailure" || controllerScenario=="expireFinal" || controllerScenario=="cancelFinal"){
    CHECK(modelSaves==1 && gitUpdateControllerPaused() && restartPending());
    if(controllerScenario=="expireFinal")++localSecurity.configurationGeneration;
    if(controllerScenario=="cancelFinal")cancelRestart();
    Fake::now+=500;controllerPump();
    if(controllerScenario=="expireFinal" || controllerScenario=="cancelFinal"){
      for(unsigned i=0;i<3;++i)controllerPump();
      CHECK(!Fake::otaActivations && !shutdownCalls && !gitUpdate.busy() && !gitUpdateControllerPaused());return 0;
    }
    if(controllerScenario=="valid" || controllerScenario=="inactivityStage")CHECK(shutdownCalls==1 && Fake::otaActivations==1 && modelSaves==2);
    else{CHECK(!shutdownCalls && Fake::otaActivations==1 && gitUpdateControllerPaused() && gitUpdate.busy());for(unsigned i=0;i<3;++i)controllerPump();CHECK(Fake::otaActivations==1 && !resumeCalls);}
    return 0;
  }
  if(controllerScenario=="cleanupBusy"){
    CHECK(gitUpdateControllerPaused() && gitUpdate.busy() && resumeCalls);resumeBusy=false;for(unsigned i=0;i<3;++i)controllerPump();
  }
  if(controllerScenario=="staleCleanup"){
    CHECK(gitUpdateControllerPaused() && gitUpdate.busy() && queueHeld && resumeCalls==1);
    for(unsigned i=0;i<3;++i)controllerPump();
    CHECK(resumeCalls==1 && !Fake::otaActivations);return 0;
  }
  if(controllerScenario=="moving" || controllerScenario=="queued" || controllerScenario=="localPending" || controllerScenario=="radioWork")CHECK(!modelSaves && !Fake::otaBegins);
  if(controllerScenario=="saveFailure" || controllerScenario=="sourceSave" || controllerScenario=="configSave")CHECK(!Fake::otaBegins);
  if(controllerScenario=="deadlineStage")CHECK(gitUpdate.snapshot().reason==GitUpdateReason::timeout);
  if(controllerScenario=="sourceWrite" || controllerScenario=="sourceSave" || controllerScenario=="configSave")CHECK(gitUpdate.snapshot().reason==GitUpdateReason::admission);
  CHECK(!shutdownCalls && !Fake::otaActivations && !gitUpdateControllerPaused() && !gitUpdate.busy());
  CHECK(!Fake::activeClients && !Fake::activeAllocations && !queueHeld && !somfy.transceiver.held);
}

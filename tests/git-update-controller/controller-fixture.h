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
static std::string controllerScenario;
static bool filesystemReady=true,queueHeld=false,queuePending=false,localPending=false;
static bool saveOkay=true,pauseOkay=true,resumeBusy=false;
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
    bool counterEditUncertain()const{return uncertain;}
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
  CHECK(requestLocalGitUpdateCheck(2,5)==GitUpdateRequest::unavailable);
  CHECK(requestLocalGitUpdateCheck(3,5)==GitUpdateRequest::accepted);controllerPump();
  const auto candidate=gitUpdate.snapshot().candidate;CHECK(candidate.generation && gitUpdate.snapshot().canInstall);
  uint64_t operation=0;
  CHECK(requestLocalGitUpdateInstall(candidate,3,4,operation)==GitUpdateRequest::unavailable && !operation);
  CHECK(requestLocalGitUpdateInstall(candidate,3,5,operation)==GitUpdateRequest::accepted && operation==candidate.generation);
  if(controllerScenario=="moving"){somfy.shades[0].id=1;somfy.shades[0].idle=false;}
  if(controllerScenario=="queued")queuePending=true;
  if(controllerScenario=="localPending")localPending=true;
  if(controllerScenario=="radioWork")somfy.transceiver.work=true;
  if(controllerScenario=="pauseFailure")pauseOkay=false;
  if(controllerScenario=="saveFailure")saveOkay=false;
  if(controllerScenario=="sourceSave")onModelSave=[](){++localSecurity.generation;};
  if(controllerScenario=="configSave")onModelSave=[](){++somfy.configurationRevision;};
  if(controllerScenario=="activationFailure")Fake::failActivate=true;
  if(controllerScenario=="cancelWrite" || controllerScenario=="cleanupBusy" || controllerScenario=="staleCleanup"){
    resumeBusy=controllerScenario=="cleanupBusy";
    Fake::onWrite=[](){CHECK(cancelLocalGitUpdate(3,5));loopGitUpdateController();CHECK(gitUpdateControllerPaused() && !resumeCalls);if(controllerScenario=="staleCleanup")somfy.transceiver.held=false;};
  }
  if(controllerScenario=="sourceWrite")Fake::onWrite=[](){localSecurity.allowed=false;loopGitUpdateController();CHECK(gitUpdateControllerPaused() && !resumeCalls);};
  for(unsigned i=0;i<5;++i)controllerPump();
  CHECK(!shutdownCalls && !Fake::otaActivations);
  if(controllerScenario=="valid" || controllerScenario=="activationFailure" || controllerScenario=="expireFinal" || controllerScenario=="cancelFinal"){
    CHECK(modelSaves==1 && gitUpdateControllerPaused() && restartPending());
    if(controllerScenario=="expireFinal")localSecurity.allowed=false;
    if(controllerScenario=="cancelFinal")cancelRestart();
    Fake::now+=500;controllerPump();
    if(controllerScenario=="expireFinal" || controllerScenario=="cancelFinal"){
      for(unsigned i=0;i<3;++i)controllerPump();
      CHECK(!Fake::otaActivations && !shutdownCalls && !gitUpdate.busy() && !gitUpdateControllerPaused());return 0;
    }
    if(controllerScenario=="valid")CHECK(shutdownCalls==1 && Fake::otaActivations==1 && modelSaves==2);
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
  CHECK(!shutdownCalls && !Fake::otaActivations && !gitUpdateControllerPaused() && !gitUpdate.busy());
  CHECK(!Fake::activeClients && !Fake::activeAllocations && !queueHeld && !somfy.transceiver.held);
}

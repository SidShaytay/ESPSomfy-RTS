#include "FrequencyScanService.h"
#include "FrequencyScanAdmission.h"
#include "Somfy.h"
#include "ConfigSettings.h"
#include "GitUpdate.h"
#include "LoopQueue.h"
#include "SafeRestart.h"
#include "ModelIdleForAdministration.h"
#include "ProtoSettings.h"
#include "SettingsRevisions.h"
#include "outputs/ProtoSocket.h"
#include <esp_timer.h>
#include <esp_random.h>
#ifdef ESPSOMFY_LOCAL_UI
#include "LocalUiAuthorizationAdapter.h"
#include "LocalOperationAdmission.h"
#endif
extern SomfyShadeController somfy;
extern ConfigSettings settings;
extern LoopQueue loopQueue;
namespace FrequencyScan {
namespace {
uint64_t now(){return uint64_t(esp_timer_get_time())/1000;}
bool trusted(){return settings.settingsUsable()&&somfy.modelLoad.usable()&&somfy.transceiver.config.loadStatus.usable();}
class DriverBackend final:public Backend {
public:
 bool current(const Source &source)override {
  if(!trusted()||source.securityRevision!=securityConfigurationRevision()||
     source.radioRevision!=deviceSettingsRevision(settings_section_t::radio))return false;
  if(source.kind==SourceKind::web)return source.client&&protoSocket.acceptsClientMutation(source.client);
#ifdef ESPSOMFY_LOCAL_UI
  const auto auth=LocalUi::pollAuthorization();
  return !LocalUi::localControlsFrozen()&&auth.allowed&&source.session==auth.generation&&source.configuration==auth.configurationGeneration;
#else
  return false;
#endif
 }
 bool acquire(GitUpdateOwnerToken &owner)override {
  return trusted()&&!restartPending()&&!loopQueue.pending()&&modelIdleForAdministration(somfy)&&
      !somfy.transceiver.counterEditUncertain()&&gitUpdate.acquireExternal(GitUpdateExternalOwner::configuration,owner);
 }
 bool owns(const GitUpdateOwnerToken &owner)override{return gitUpdate.ownsExternal(owner);}
 bool begin()override{return somfy.transceiver.beginFrequencyScan();}
 void stop()override{somfy.transceiver.endFrequencyScan();}
 bool active()override{return frequencyScanActive();}
 bool sample(Sample &out)override{return somfy.transceiver.readFrequencyScan(out);}
 bool release(const GitUpdateOwnerToken &owner)override{return gitUpdate.releaseExternal(owner);}
};
uint64_t nonce(){
 for(unsigned attempt=0;attempt<4;++attempt){const uint64_t value=(uint64_t(esp_random())<<32)|esp_random();if(value)return value;}
 return 0;
}
Session &service(){static DriverBackend backend;static Session instance(backend,nonce());return instance;}
}
Source webSource(uint32_t client){return {SourceKind::web,client,0,0,securityConfigurationRevision(),deviceSettingsRevision(settings_section_t::radio)};}
Source localSource(uint32_t session,uint32_t configuration){return {SourceKind::lcd,0,session,configuration,securityConfigurationRevision(),deviceSettingsRevision(settings_section_t::radio)};}
bool begin(const Source &source,FrequencyScanToken &out){return service().start(source,now(),out);}
bool stop(const Source &source,const FrequencyScanToken &token){return service().stop(token,source,now());}
void poll(){service().poll(now());}
Snapshot status(){return service().snapshot();}
bool peak(const Source &source,const FrequencyScanToken &token,float &frequency){return service().peak(token,source,frequency);}
}

static bool pinEligible=true;
RadioPinContext currentRadioPinContext(){return {0,true};}
RadioPinContext withRetainedEthernetClaims(RadioPinContext value){return value;}
bool radioPinConfigurationAvailable(const transceiver_config_t &,RadioPinContext context){return context.qualified && pinEligible;}
bool currentRadioPinConfigurationAvailable(const transceiver_config_t &cfg){return radioPinConfigurationAvailable(cfg,currentRadioPinContext());}
// Included after the real radio/pause fixture and extracted production methods.
static unsigned saves=0,applies=0;
static bool restarting=false,failMarker=false,failPersist=false;
static void (*duringSave)()=nullptr;
static bool cleanupAfterSaveBusy=false;
bool restartPending(){return restarting;}
settings_save_result_t transceiver_config_t::save(){++saves;if(duringSave)duringSave();return failPersist?settings_save_result_t{settings_save_state_t::failed,"CC1101"}:settings_save_result_t{};}
settings_save_result_t transceiver_config_t::apply(){++applies;somfy.transceiver._available=!failMarker;radioInit=!failMarker;if(failMarker)settingsDurability().markUncertain(settingsNamespaceBit(SettingsNamespace::radio));return failMarker?settings_save_result_t{settings_save_state_t::partial,"radioInit"}:settings_save_result_t{};}
#include "production-radio-settings.inc"
int main(int argc,char **argv){assert(argc==2);const std::string mode=argv[1];fresh();for(auto &g:somfy.groups)g.id=255;
 loopQueue.work=0;transceiver_config_t next=somfy.transceiver.config;next.RXPin=9;
 GitUpdateOwnerToken foreign;
 if(mode=="owner")assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::configuration,foreign));
 if(mode=="pinConflict")pinEligible=false;
 if(mode=="movement")somfy.shades[0].id=1,somfy.shades[0].idle=false;
 if(mode=="held")somfy.shades[0].id=1,somfy.shades[0].idle=false;
 if(mode=="groupCompleted"){somfy.groups[0].id=1;somfy.groups[0].direction=1;somfy.groups[0].linkedShades[31]=1;somfy.shades[0].id=1;}
 if(mode=="group")somfy.groups[0].id=1,somfy.groups[0].direction=1;
 if(mode=="pending")loopQueue.work=1;
 if(mode=="producer")loopQueue.producers=1;
 if(mode=="radio") {somfy_tx_job_t job;job.frame.remoteAddress=55;job.frame.cmd=somfy_commands::Up;assert(somfy.transceiver.sched.submit(job,false,0,0));}
 if(mode=="scan")rxmode=3;
 if(mode=="restart")restarting=true;
 if(mode=="untrusted")somfy.modelLoad.trusted=false;
 if(mode=="lateMovement")beforeDetach=[]{somfy.shades[0].id=1;somfy.shades[0].idle=false;};
 if(mode=="marker")failMarker=true;
 if(mode=="persist")failPersist=true;
 if(mode=="staleCleanup")duringSave=[]{++somfy.transceiver.installGeneration;};
 if(mode=="busyCleanup")duringSave=[]{radioBusy=true;cleanupAfterSaveBusy=true;};
 const bool refused=mode!="success"&&mode!="groupCompleted"&&mode!="marker"&&mode!="persist"&&mode!="busyCleanup"&&mode!="staleCleanup";
 const auto scanSeal=somfy.transceiver.sched.hasWork();const auto scanGeneration=somfy.transceiver.installGeneration;const auto result=somfy.transceiver.setConfig(next);
 if(mode=="scan")assert(rxmode==3 && somfy.transceiver.installGeneration==scanGeneration && somfy.transceiver.sched.hasWork()==scanSeal && !somfy.transceiver._administrationPaused.load());
 if(refused){assert(!result.saved()&&!saves&&!applies);assert(somfy.transceiver.config.RXPin==8);assert(loopQueue.work==unsigned(mode=="pending"));assert(!loopQueue.restartSealed.load());}
 else if(mode=="persist")assert(!result.saved()&&saves==1&&!applies&&somfy.transceiver.config.RXPin==8);
 else if(mode=="staleCleanup" || mode=="busyCleanup")assert(saves==1&&!applies&&somfy.transceiver.config.RXPin==8&&result.radioNamespaceVerified&&result.state==settings_save_state_t::partial);
 else {assert(saves==1&&applies==1&&somfy.transceiver.config.RXPin==9);if(mode=="marker")assert(!result.saved()&&result.state==settings_save_state_t::partial&&result.liveApplied&&!settingsDurability().status().restartAllowed());else if(mode!="staleCleanup")assert(result.saved());}
 if(mode=="staleCleanup") {
  assert(!result.saved() && result.radioCleanupFault && gitUpdate.busy() && loopQueue.restartSealed.load());
  const auto generation=somfy.transceiver.installGeneration;
  serviceRadioSettingsCleanup();assert(gitUpdate.busy()&&loopQueue.restartSealed.load());
  assert(!somfy.transceiver.setConfig(next).saved()&&saves==1&&!applies);
  assert(somfy.transceiver.installGeneration==generation);puts("Stale cleanup capability remains quarantined without releasing replacement resources");return 0;
 }
 if(foreign.generation)assert(gitUpdate.releaseExternal(foreign));
 if(cleanupAfterSaveBusy){assert(gitUpdate.busy()&&loopQueue.restartSealed.load());assert(!somfy.transceiver.setConfig(next).saved()&&saves==1);radioBusy=false;serviceRadioSettingsCleanup();assert(!gitUpdate.busy()&&!loopQueue.restartSealed.load());}
 assert(!gitUpdate.busy());puts("Actual live radio save admission/pause/cleanup outcome passed");
}

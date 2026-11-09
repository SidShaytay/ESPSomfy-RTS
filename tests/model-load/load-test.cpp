#define HAS_ETHERNET 0
#include "Arduino.h"
#include "ConfigRecords.h"
#include "ConfigValidation.h"
#include "ModelLoadSnapshot.h"
#include "ModelLoadTrust.h"
#include "ModelCommitResult.h"
#include "CounterLoad.h"
#include "RestoreResult.h"
#include "ConfigSettings.h"
#include "Transceiver.h"
#include "SelectedSettingsRestore.h"
#include "SettingsApply.h"
#include "QualifiedSettings.h"
#include "GitUpdate.h"
#include "SettingsDurability.h"
#include "RollingCode.h"
#include <nvs.h>
#include "ShadeBehaviour.h"
#include "RadioUpdateActivity.h"
#include "ModelIdleForAdministration.h"
#include "ModelEditor.h"
#include "ShadeGpioPolicy.h"
#define GPIO_IS_VALID_OUTPUT_GPIO(pin) ((pin)<48)
#include <cassert>
#include <string>
#include <unistd.h>
#include <cstdio>
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
struct DummyFile {explicit operator bool()const{return false;}bool isDirectory(){return false;}void close(){}void flush(){}uint32_t position(){return 0;}};
struct {unsigned writeOpens=0;std::string root;const char *mountpoint(){return root.c_str();}DummyFile open(const char *,const char *){++writeOpens;return {};}} LittleFS;
struct Preferences {
 nvs_handle_t handle=0;bool begin(const char *space){return nvs_open(space,NVS_READWRITE,&handle)==ESP_OK;}
 uint16_t getUShort(const char *key,uint16_t def=0){uint16_t value=def;return nvs_get_u16(handle,key,&value)==ESP_OK?value:def;}
 size_t putUShort(const char *key,uint16_t value){return nvs_set_u16(handle,key,value)==ESP_OK?2:0;}
 void end(){nvs_close(handle);}
};
struct SomfyRemote {
 uint32_t address=0;uint16_t lastRollingCode=0;char prefKey[11]{};
 void setRemoteAddress(uint32_t value){address=value;snprintf(prefKey,sizeof(prefKey),"_%lu",static_cast<unsigned long>(value));}
 uint32_t getRemoteAddress(){return address;}char *getRemotePrefId(){return prefKey;}
};
struct SomfyLinkedRemote:SomfyRemote{};
struct NoMotorEffects:MotorLink,ShadeEvents {
 unsigned sends=0,events=0;
 send_result_t send(somfy_commands,uint8_t,uint8_t,uint16_t,bool)override{++sends;return {};}
 void onChanged(uint16_t)override{++events;}void onCommand(somfy_commands,const char *,uint32_t)override{++events;}
 void onSaveWanted()override{++events;}void onSensorFlags()override{++events;}
};
struct SomfyShade:SomfyRemote,shade_record_t {
 using SomfyRemote::lastRollingCode;SomfyLinkedRemote linkedRemotes[SOMFY_MAX_LINKED_REMOTES];
 ShadeSettings behaviorSettings;RemoteSettings behaviorRemote;somfy_frame_t frame;NoMotorEffects effects;ShadeBehaviour behaviour;uint32_t gpioRelease=0;
 SomfyShade():behaviour(behaviorSettings,behaviorRemote,frame,effects,effects,shadeId,name){}
 bool idleForAdministration()const;
 uint8_t getShadeId(){return shadeId;}void setShadeId(uint8_t value){shadeId=value;}
 void clear(){shadeId=255;}void setFlags(uint8_t value){flags=value;}
 void restore(float a,float b,float c,float d){currentPos=a;currentTiltPos=b;myPos=c;myTiltPos=d;behaviour.restore(a,b,c,d);}
};
struct SomfyGroup:SomfyRemote,group_record_t {
 using SomfyRemote::lastRollingCode;int8_t direction=0;somfy_frame_t lastFrame;
 uint8_t getGroupId(){return groupId;}void setGroupId(uint8_t value){groupId=value;}void clear(){groupId=255;}void compressLinkedShadeIds(){}
};
static shade_record_t shadeRecord(SomfyShade *shade){return *shade;}
static group_record_t groupRecord(SomfyGroup *group){return *group;}

static bool restoreAllowed=true;static int expireAtReads=-1;static unsigned afterReads=0;
constexpr GitUpdateOwnerToken RestoreOwner{7,GitUpdateExternalOwner::restore};
GitUpdateClass gitUpdate;
bool GitUpdateClass::transferRestoreClaimed(const GitUpdateOwnerToken &owner,uint64_t){
 if(!restoreAllowed||owner.generation!=RestoreOwner.generation||owner.kind!=RestoreOwner.kind)return false;
 if(expireAtReads>=0&&FakeNvs::reads>=expireAtReads)return ++afterReads<2;
 return true;
}
bool GitUpdateClass::ownsExternal(const GitUpdateOwnerToken &owner)const{return restoreAllowed&&owner.generation==7&&owner.kind==GitUpdateExternalOwner::restore;}
int64_t esp_timer_get_time(){return 1000000;}
static void resetTaskWdt(){}
static bool retireAfterWrite=false;
static void delay(unsigned long){if(retireAfterWrite && FakeNvs::sets)restoreAllowed=false;}
void invalidateDeviceSettingsRevisions(){}
static int expectedSetupPin=-1;
static unsigned settingEffects=0;
ConfigSettings settings;
namespace BoardProfile{bool usesPin(uint8_t){return false;}}
bool NTPSettings::apply(){return true;}
struct TestRadio:Transceiver {
 unsigned starts=0;bool activityKnown=true;
 RadioUpdateActivity activity=[](){RadioUpdateActivity a;a.observed=true;a.schedulerIdle=true;return a;}();
 TestRadio(){qualifySettings(config);}
 bool installPauseHeld(const RadioInstallToken &token)const{return token.generation==1&&activityKnown&&activity.quiescent();}
 bool updateActivity(RadioUpdateActivity &out)const{if(!activityKnown)return false;out=activity;return true;}
 bool begin(){++starts;return true;}bool counterEditUncertain(){return false;}
};
static settings_record_t settingsRecord(){return {};}
static net_record_t netRecord(){return {};}
template<class T>static trans_record_t transRecord(const T &){return {};}
static void applySettingsRecord(const settings_record_t &){++settingEffects;}
static void applyNetRecord(const net_record_t &,restore_options_t &,bool){++settingEffects;}
static void applyTransRecord(const trans_record_t &,transceiver_config_t &){++settingEffects;}
struct SomfyShadeController{
 unsigned adopted=0,repeatersRead=0,commits=0,gpio=0;
 SomfyShade shades[SOMFY_MAX_SHADES];SomfyGroup groups[SOMFY_MAX_GROUPS];
 SomfyShade *getShadeById(uint8_t id){for(auto &shade:shades)if(id&&id!=255&&shade.getShadeId()==id)return &shade;return nullptr;}
 bool begin();uint32_t repeaters[SOMFY_MAX_REPEATERS]{};
 ModelLoadTrust modelLoad;uint64_t configurationRevision=1,restoreRevision=1;bool saveOK=true;
 TestRadio transceiver;
 ModelCommitResult commit(){assert(modelLoad.mayCommit());++commits;return {saveOK?ModelCommitState::saved:ModelCommitState::failed};}
 void configureGPIOs(){assert(modelLoad.usable());if(expectedSetupPin>=0)assert(transceiver.config.TXPin==expectedSetupPin);++gpio;}
};
SomfyShadeController somfy;
struct {unsigned queued=0;unsigned pending()const{return queued;}}loopQueue;
static bool restartQueued=false;static bool restartPending(){return restartQueued;}
struct ConfigFile:ConfigReader {
 DummyFile file;String *_ramBuf=nullptr;bool readOnly=false,_writeFailed=false,_opened=false;
 ModelLoadSnapshot snapshot;ModelLoadStatus snapshotStatus=ModelLoadStatus::unknown;size_t readPosition=0;bool readFailed=false;
 config_header_t header;
 bool begin(const char *,bool=false);void end();bool isOpen(){return _opened;}
 bool readByte(uint8_t &)override;uint32_t position()override;bool seekRead(uint32_t);size_t readSize()const{return snapshot.size();}bool readHeader();
};
static bool failCounterAllocation=false;
static void *allocateCounters(size_t n){return failCounterAllocation?nullptr:modelLoadIo().allocate(n);}
static void releaseCounters(void *p,size_t n){modelLoadIo().release(p,n);}
struct ShadeConfigFile:ConfigFile {
 CounterLoad counters{allocateCounters,releaseCounters};bool prepareCounters(CounterLoadControl control={});void readShade(SomfyShade *);void readGroup(SomfyGroup *);
 bool begin(bool=false);bool begin(const char *,bool=false);
 bool validateRestoreRecords(const restore_options_t &);
 bool validateRestoreGPIOs(SomfyShadeController *,bool,const class SelectedSettingsRestore &);
 RestoreApplyResult restoreFile(SomfyShadeController *,const char *,restore_options_t &,const GitUpdateOwnerToken &,const RadioInstallToken &);
 bool validate();bool loadFile(SomfyShadeController *,const char *filename="/shades.cfg");
 static ModelLoadStatus loadBoot(SomfyShadeController *);
 void readModel(SomfyShadeController *s){
  ++s->adopted;const auto reads=FakeNvs::reads,sets=FakeNvs::sets;
  for(unsigned i=0;i<header.roomRecords;++i){room_record_t r;assert(readRoomRecord(*this,r));}
  for(unsigned i=0;i<header.shadeRecords;++i)readShade(&s->shades[i]);
  for(unsigned i=0;i<header.groupRecords;++i)readGroup(&s->groups[i]);
  assert(FakeNvs::reads==reads&&FakeNvs::sets==sets); // Adoption has no NVS I/O.
 }

 void readRepeaters(SomfyShadeController *s){++s->repeatersRead;uint32_t r[SOMFY_MAX_REPEATERS];assert(readRepeaterRecord(*this,r));}
};
static bool radioApplyPartial=false;
static SomfyShadeController *expectedApplyModel=nullptr;
bool Transceiver::installPauseHeld(const RadioInstallToken &pause)const{return static_cast<const TestRadio *>(this)->installPauseHeld(pause);}
settings_save_result_t Transceiver::setConfigWhilePaused(const transceiver_config_t &next,const RadioInstallToken &pause,const GitUpdateOwnerToken &owner){
 assert(installPauseHeld(pause)&&gitUpdate.ownsExternal(owner));auto copy=next;const auto persisted=copy.save();if(!persisted.saved())return persisted;
 config=copy;config.radioInit=!radioApplyPartial;if(radioApplyPartial){settingsDurability().markUncertain(settingsNamespaceBit(SettingsNamespace::radio));return {settings_save_state_t::partial,"radioInit",true};}
 return {settings_save_state_t::saved,nullptr,true};
}
struct RestoreEffects:SettingsEffects {
 bool wifiClientsPresent()const override{return true;}
 void hostnameChanged()override{++settingEffects;}void timeChanged()override{++settingEffects;}void mqttChanged()override{++settingEffects;}
 settings_save_result_t saveRadio(JsonObject)override{assert(false);return {};}
 RestartResult completed(settings_section_t,bool reboot)override{assert(!reboot);return {};}
};
static RestoreSettingsResult applyPreparedSettingsRestore(SelectedSettingsRestore &stage,Transceiver &radio,const GitUpdateOwnerToken &owner,const RadioInstallToken &token){
 assert(FakeNvs::handles.empty());if(expectedApplyModel)assert(expectedApplyModel->commits==1&&expectedApplyModel->saveOK);
 RestoreEffects effects;return stage.apply(settings,radio,owner,token,effects);
}
static void cooperateCounterLoad(const void *);
static bool continueRestoreCounterLoad(const void *);
static bool restoreIdle(SomfyShadeController *,const RadioInstallToken &);
static bool restoreExecutionHolds(const GitUpdateOwnerToken &,const RadioInstallToken &radio){return radio.generation==1;}
#include "production-load.inc"
struct Writer:ConfigWriter {std::string bytes;bool writeBytes(const uint8_t *p,size_t n)override{bytes.append(reinterpret_cast<const char *>(p),n);return true;}};
static std::string validModel(){Writer w;config_header_t h;h.version=SHADE_HDR_VER;h.length=SHADE_HDR_SIZE;h.shadeRecordSize=SHADE_REC_SIZE;h.groupRecordSize=GROUP_REC_SIZE;h.roomRecordSize=ROOM_REC_SIZE;h.roomRecords=h.shadeRecords=h.groupRecords=0;h.repeaterRecords=1;h.repeaterRecordSize=REPEATER_REC_SIZE;h.settingsRecordSize=h.netRecordSize=h.transRecordSize=0;assert(writeConfigHeader(w,h));uint32_t repeaters[SOMFY_MAX_REPEATERS]{};assert(writeRepeaterRecord(w,repeaters));return w.bytes;}
static std::string aliasesModel(bool highFirst,uint16_t groupFloor=50,uint8_t version=SHADE_HDR_VER,uint32_t address=100){
 Writer w;config_header_t h;h.version=version;h.length=SHADE_HDR_SIZE;h.shadeRecordSize=SHADE_REC_SIZE;h.groupRecordSize=GROUP_REC_SIZE;h.roomRecordSize=ROOM_REC_SIZE;
 h.shadeRecords=2;h.groupRecords=1;h.repeaterRecords=1;h.repeaterRecordSize=REPEATER_REC_SIZE;
 assert(writeConfigHeader(w,h));
 for(unsigned i=0;i<2;++i){shade_record_t rec;rec.shadeId=i+1;rec.remoteAddress=address;rec.bitLength=56;rec.lastRollingCode=(i==unsigned(highFirst))?300:20;rec.linkedRemotes[0]=100;rec.linkedRemotes[1]=101;assert(writeShadeRecord(w,rec));}
 group_record_t group;group.groupId=1;group.remoteAddress=100;group.bitLength=56;group.lastRollingCode=groupFloor;assert(writeGroupRecord(w,group));uint32_t repeaters[SOMFY_MAX_REPEATERS]{};assert(writeRepeaterRecord(w,repeaters));return w.bytes;
}
static std::string gpioModel(uint8_t up,uint8_t down,uint8_t my,radio_proto protocol=radio_proto::GP_Remote,bool withRadio=false){
 Writer w;config_header_t h;h.version=SHADE_HDR_VER;h.length=SHADE_HDR_SIZE;h.shadeRecordSize=SHADE_REC_SIZE;h.groupRecordSize=GROUP_REC_SIZE;h.roomRecordSize=ROOM_REC_SIZE;h.shadeRecords=1;
 trans_record_t radio;radio.enabled=true;radio.TXPin=27;
 if(withRadio){ConfigCounter count;assert(writeTransRecord(count,radio));h.transRecordSize=count.bytes;}
 assert(writeConfigHeader(w,h));shade_record_t rec;rec.shadeId=1;rec.proto=protocol;rec.remoteAddress=100;rec.lastRollingCode=200;rec.gpioUp=up;rec.gpioDown=down;rec.gpioMy=my;assert(writeShadeRecord(w,rec));if(withRadio)assert(writeTransRecord(w,radio));return w.bytes;
}
static std::string fullSettingsModel(){
 const auto base=aliasesModel(false);
 struct Input:ConfigReader {const std::string &text;size_t at=0;explicit Input(const std::string &s):text(s){}bool readByte(uint8_t &v)override{if(at==text.size())return false;v=text[at++];return true;}uint32_t position()override{return at;}}input(base);
 config_header_t header;assert(readConfigHeader(input,header));const auto body=base.substr(input.position());
 Writer tail;settings_record_t general;strcpy(general.hostname,"restored");strcpy(general.ntpServer,"restore.clock");strcpy(general.posixZone,"UTC0");assert(writeSettingsRecord(tail,general));header.settingsRecordSize=tail.bytes.size();
 const auto netStart=tail.bytes.size();net_record_t network;network.connType=1;strcpy(network.mqttProtocol,"mqtt");strcpy(network.mqttHostname,"restored-broker");strcpy(network.mqttRootTopic,"restored-root");strcpy(network.mqttDiscoTopic,"discovery");assert(writeNetRecord(tail,network,false));header.netRecordSize=tail.bytes.size()-netStart;
 const auto radioStart=tail.bytes.size();trans_record_t radio;radio.enabled=true;radio.TXPin=27;assert(writeTransRecord(tail,radio));header.transRecordSize=tail.bytes.size()-radioStart;
 Writer writer;assert(writeConfigHeader(writer,header));return writer.bytes+body+tail.bytes;
}
static void stored(uint32_t address,uint16_t value){char key[32];snprintf(key,sizeof(key),"ShadeCodes/_%lu",static_cast<unsigned long>(address));FakeNvs::values[key]={FakeNvs::Type::u16,{uint8_t(value),uint8_t(value>>8)}};}
static void retireRestore(){restoreAllowed=false;}
static void writeAt(const char *path,const std::string &bytes){FILE *f=fopen((LittleFS.root+path).c_str(),"wb");assert(f);assert(fwrite(bytes.data(),1,bytes.size(),f)==bytes.size());assert(fclose(f)==0);}
static void write(const std::string &bytes){writeAt("/shades.cfg",bytes);}
static std::string readAt(const char *path){FILE *f=fopen((LittleFS.root+path).c_str(),"rb");assert(f);std::string bytes;int c;while((c=fgetc(f))!=EOF)bytes+=char(c);assert(!ferror(f)&&fclose(f)==0);return bytes;}
int main(){char dir[]="/tmp/espsomfy-model-load.XXXXXX";assert(mkdtemp(dir));LittleFS.root=dir;qualifySettings(settings);SomfyShadeController model;
 ShadeConfigFile noUncheckedWrite;assert(!noUncheckedWrite.begin(false)&&LittleFS.writeOpens==0);
 noteModelFilesystemMounted(false);assert(ShadeConfigFile::loadBoot(&model)==ModelLoadStatus::unavailable&&!model.adopted);
 noteModelFilesystemMounted(true);SomfyShadeController empty;assert(empty.begin()&&empty.modelLoad.usable()&&empty.gpio==1&&!empty.commits);assert(ShadeConfigFile::loadBoot(&model)==ModelLoadStatus::absent&&!model.adopted);
 const auto valid=validModel();write(valid);assert(ShadeConfigFile::loadBoot(&model)==ModelLoadStatus::loaded&&model.adopted==1&&model.repeatersRead==1);
 model.adopted=model.repeatersRead=0;
 write(valid.substr(0,valid.size()-1));assert(ShadeConfigFile::loadBoot(&model)==ModelLoadStatus::unavailable&&!model.adopted&&!model.repeatersRead);
 write(valid+"unexpected\n");assert(ShadeConfigFile::loadBoot(&model)==ModelLoadStatus::unavailable&&!model.adopted);
 write(valid.substr(0,3));SomfyShadeController failed;assert(!failed.begin()&&!failed.modelLoad.usable()&&!failed.gpio&&!failed.commits&&failed.transceiver.starts==1);assert(ShadeConfigFile::loadBoot(&model)==ModelLoadStatus::unavailable&&!model.adopted);
 write(valid);
 for(unsigned mode=0;mode<3;++mode){SomfyShadeController bad;bad.modelLoad.fail();ShadeConfigFile file;assert(file.begin("/shades.cfg",true));restore_options_t opts;opts.shades=mode==1;opts.repeaters=mode==2;
   assert(!file.restoreFile(&bad,"/shades.cfg",opts,RestoreOwner,RadioInstallToken{1}).complete);assert(!bad.adopted&&!bad.commits&&!bad.gpio&&!bad.modelLoad.usable());file.end();}
 for(bool success:{false,true}){SomfyShadeController bad;bad.modelLoad.fail();bad.saveOK=success;ShadeConfigFile file;assert(file.begin("/shades.cfg",true));restore_options_t opts;opts.shades=opts.repeaters=true;
   const auto result=file.restoreFile(&bad,"/shades.cfg",opts,RestoreOwner,RadioInstallToken{1});assert(result.complete==success&&result.modelApplied&&result.modelFile.saved()==success);assert(bad.adopted==1&&bad.commits==1);assert(bad.modelLoad.usable()==success);assert(bad.gpio==unsigned(success));file.end();}
 assert(!settingEffects);
 for(bool highFirst:{false,true}) for(uint16_t groupFloor:{uint16_t(50),uint16_t(500)}) {
  FakeNvs::reset();stored(100,100);stored(101,900);write(aliasesModel(highFirst,groupFloor));SomfyShadeController loaded;
  assert(loaded.begin()&&loaded.modelLoad.usable()&&loaded.adopted==1&&loaded.gpio==1);
  const uint16_t maximum=groupFloor>300?groupFloor:300;
  for(unsigned i=0;i<2;++i)assert(loaded.shades[i].lastRollingCode==maximum&&loaded.shades[i].linkedRemotes[0].lastRollingCode==maximum&&loaded.shades[i].linkedRemotes[1].lastRollingCode==900);
  assert(loaded.groups[0].lastRollingCode==maximum&&FakeNvs::sets==1&&FakeNvs::writeOpens==1&&FakeNvs::readOnlyOpens==2);
 }
 FakeNvs::reset();stored(100,100);stored(101,900);write(aliasesModel(true,500,22));SomfyShadeController legacy;
 assert(legacy.begin()&&legacy.groups[0].lastRollingCode==300); // v22 has no authoritative group code.
 // Restore keeps the legacy v22 group default while validating present fields.
 FakeNvs::reset();stored(100,100);stored(101,900);writeAt("/shades.tmp",aliasesModel(true,500,22));restoreAllowed=true;
 {SomfyShadeController live;live.modelLoad.bootResult(ModelLoadStatus::loaded);ShadeConfigFile file;restore_options_t opts;opts.shades=true;
  const auto result=file.restoreFile(&live,"/shades.tmp",opts,RestoreOwner,RadioInstallToken{1});
  assert(result.complete&&result.modelApplied&&live.groups[0].lastRollingCode==300);}
 for(unsigned failure=0;failure<9;++failure){
  FakeNvs::reset();stored(100,100);stored(101,900);write(aliasesModel(false));
  if(failure==0)FakeNvs::failOpen=1;if(failure==1)FakeNvs::failRead=1;if(failure==2)FakeNvs::failRead=2;
  if(failure==3)FakeNvs::failSet=1;if(failure==4)FakeNvs::failCommit=1;if(failure==5)FakeNvs::failOpen=3;if(failure==6)FakeNvs::failRead=3;if(failure==7)FakeNvs::corruptRead=3;if(failure==8)FakeNvs::failOpen=2;
  SomfyShadeController failed;assert(!failed.begin()&&!failed.modelLoad.usable()&&!failed.modelLoad.publishable()&&!failed.adopted&&!failed.gpio&&!failed.commits);
  if(failure<3)assert(FakeNvs::writeOpens==0&&FakeNvs::sets==0);
  assert(FakeNvs::handles.empty());
 }

 for(bool afterWrite:{false,true}){
  FakeNvs::reset();stored(100,100);stored(101,900);write(aliasesModel(false));SomfyShadeController bad;restoreAllowed=true;retireAfterWrite=afterWrite;if(!afterWrite)FakeNvs::beforeGet=retireRestore;
  ShadeConfigFile file;assert(file.begin("/shades.cfg",true));restore_options_t opts;opts.shades=opts.repeaters=true;
  assert(!file.restoreFile(&bad,"/shades.cfg",opts,RestoreOwner,RadioInstallToken{1}).complete&&!bad.modelLoad.usable()&&!bad.adopted&&!bad.gpio&&!bad.commits);
  assert(FakeNvs::sets==unsigned(afterWrite)&&FakeNvs::commits==0&&FakeNvs::handles.empty());file.end();
 }
 restoreAllowed=true;retireAfterWrite=false;FakeNvs::reset();
 // Refusing an import before any write/adoption preserves a previously trusted
 // running model, its identities/history and the canonical file.
 for(unsigned cause=0;cause<5;++cause){
  FakeNvs::reset();stored(100,100);stored(101,900);write(valid);writeAt("/shades.tmp",cause==4?valid.substr(0,3):aliasesModel(false,50,SHADE_HDR_VER,cause==1?0x1000000:100));
  SomfyShadeController live;live.modelLoad.bootResult(ModelLoadStatus::loaded);live.shades[0].setShadeId(7);live.shades[0].setRemoteAddress(777);live.shades[0].lastRollingCode=80;
  restoreAllowed=true;retireAfterWrite=false;if(cause==0)FakeNvs::beforeGet=retireRestore;if(cause==2)FakeNvs::failRead=1;failCounterAllocation=cause==3;
  ShadeConfigFile file;assert(file.begin("/shades.tmp",true));restore_options_t opts;opts.shades=opts.repeaters=true;
  const auto result=file.restoreFile(&live,"/shades.tmp",opts,RestoreOwner,RadioInstallToken{1});assert(!result.complete&&!result.modelApplied&&!result.counterWriteAttempted&&!result.modelFile.saved()&&live.modelLoad.usable()&&live.modelLoad.publishable());
  assert(!live.adopted&&!live.commits&&!live.gpio&&live.configurationRevision==1&&live.restoreRevision==1);
  assert(live.shades[0].getShadeId()==7&&live.shades[0].getRemoteAddress()==777&&live.shades[0].lastRollingCode==80);
  assert(!FakeNvs::writeOpens&&!FakeNvs::sets&&FakeNvs::handles.empty()&&readAt("/shades.cfg")==valid);file.end();failCounterAllocation=false;
 }
 // Expiry after READY must retain whether verified promotions happened.
 // Both callers that opened the source and internally opened sources qualify.
 for(bool promoted:{false,true})for(bool preopened:{false,true}){
  FakeNvs::reset();stored(100,promoted?100:900);stored(101,900);write(valid);writeAt("/shades.tmp",aliasesModel(false));
  restoreAllowed=true;expireAtReads=promoted?4:2;afterReads=0;
  SomfyShadeController live;live.modelLoad.bootResult(ModelLoadStatus::loaded);live.shades[0].setShadeId(7);live.shades[0].setRemoteAddress(777);live.shades[0].lastRollingCode=80;
  ShadeConfigFile file;if(preopened)assert(file.begin("/shades.tmp",true));restore_options_t opts;opts.shades=opts.repeaters=true;
  const auto result=file.restoreFile(&live,"/shades.tmp",opts,RestoreOwner,RadioInstallToken{1});assert(!result.complete&&!result.modelApplied&&result.counterWriteAttempted==promoted&&result.partial()==promoted&&live.modelLoad.usable()==!promoted);
  assert(!live.adopted&&!live.commits&&!live.gpio&&live.shades[0].getRemoteAddress()==777&&live.shades[0].lastRollingCode==80);
  assert(FakeNvs::sets==unsigned(promoted)&&FakeNvs::commits==unsigned(promoted)&&FakeNvs::handles.empty()&&readAt("/shades.cfg")==valid);
  if(preopened)assert(file.counters.result().ready()&&file.counters.result().writeAttempted==promoted);
  file.end();expireAtReads=-1;afterReads=0;
 }
 // Raw fields must be validated before legacy numeric casts can wrap them.
 for(unsigned malformed=0;malformed<3;++malformed){
  const char *only=getenv("RESTORE_RAW_CASE");if(only&&unsigned(atoi(only))!=malformed)continue;
  auto bytes=gpioModel(17,40,255);const unsigned field=malformed==0?22+SOMFY_MAX_LINKED_REMOTES:malformed==1?3:12+SOMFY_MAX_LINKED_REMOTES;
  size_t start=SHADE_HDR_SIZE;for(unsigned i=0;i<field;++i)start=bytes.find(',',start)+1;const auto end=bytes.find(',',start);
  const char *raw=malformed==0?"257":malformed==1?"4294967396":"65536";assert(strlen(raw)==end-start);bytes.replace(start,end-start,raw);
  FakeNvs::reset();stored(100,100);writeAt("/shades.tmp",bytes);restoreAllowed=true;
  SomfyShadeController live;live.modelLoad.bootResult(ModelLoadStatus::loaded);ShadeConfigFile file;restore_options_t opts;opts.shades=true;
  const auto result=file.restoreFile(&live,"/shades.tmp",opts,RestoreOwner,RadioInstallToken{1});
  assert(!result.complete&&!result.modelApplied&&!result.counterWriteAttempted&&!FakeNvs::writeOpens&&!live.gpio&&live.modelLoad.publishable());
 }
 if(getenv("RESTORE_RAW_CASE"))return 0;
 // Projected physical policy rejects invalid pads and duplicate/unknown
 // owners before promotion, while valid joint pin changes use the candidate.
 for(unsigned mode=0;mode<4;++mode){
  FakeNvs::reset();stored(100,100);writeAt("/shades.tmp",gpioModel(mode==1?254:17,mode==0?17:40,255,radio_proto::GP_Remote,mode==3));restoreAllowed=true;

  SomfyShadeController live;live.modelLoad.bootResult(ModelLoadStatus::loaded);ShadeConfigFile file;restore_options_t opts;opts.shades=true;opts.transceiver=mode==3;
  if(mode==2)live.transceiver.config.loadStatus.state=SettingsLoadState::failed;
  if(mode==3){writeAt("/shades.tmp",gpioModel(27,40,255,radio_proto::GP_Remote,true));}
  const auto result=file.restoreFile(&live,"/shades.tmp",opts,RestoreOwner,RadioInstallToken{1});
  assert(!result.complete&&!result.modelApplied&&!result.counterWriteAttempted&&!FakeNvs::writeOpens&&!live.adopted&&!live.gpio&&live.modelLoad.publishable());
 }

 for(bool partial:{false,true}){
  FakeNvs::reset();stored(100,100);writeAt("/shades.tmp",gpioModel(17,40,255,radio_proto::GP_Remote,true));restoreAllowed=true;
  SomfyShadeController live;live.modelLoad.bootResult(ModelLoadStatus::loaded);live.transceiver.config.enabled=true;live.transceiver.config.TXPin=17;
  ShadeConfigFile file;restore_options_t opts;opts.shades=opts.transceiver=true;
  expectedSetupPin=27;expectedApplyModel=&live;radioApplyPartial=partial;
  const auto result=file.restoreFile(&live,"/shades.tmp",opts,RestoreOwner,RadioInstallToken{1});
  assert(result.complete==!partial&&result.modelApplied&&result.modelFile.saved()&&result.counterWriteAttempted&&live.gpio==1);
  assert(result.settings.persistence.liveApplied&&result.settings.persistence.saved()==!partial&&result.partial()==partial);
  expectedSetupPin=-1;expectedApplyModel=nullptr;radioApplyPartial=false;
 }
 // Legacy header decoding truncates the ID, so selected restore must prove
 // the complete untruncated token before deciding same-board ownership.
 for(bool truncatedId:{false,true}){auto malformed=gpioModel(17,40,255,radio_proto::GP_Remote,true);
  if(truncatedId){malformed.replace(4,3," 77");malformed.replace(SHADE_HDR_SIZE-10,9,"ABCDEF   X");}
  else malformed.replace(24,3,"257"); // Old uint8 conversion wraps to the existing one-record count.
  FakeNvs::reset();stored(100,100);writeAt("/shades.tmp",malformed);strcpy(settings.serverId,"ABCDEF");
  SomfyShadeController live;live.modelLoad.bootResult(ModelLoadStatus::loaded);ShadeConfigFile file;
  assert(file.begin("/shades.tmp",true)&&file.validate()&&file.seekRead(0));restore_options_t opts;opts.shades=true;
  const auto result=file.restoreFile(&live,"/shades.tmp",opts,RestoreOwner,RadioInstallToken{1});
  assert(!result.complete&&!result.modelApplied&&!FakeNvs::writeOpens&&!live.gpio&&live.modelLoad.publishable());
  settings.serverId[0]=0;
 }
 // GP_Relay's dormant MY pad is not a physically touched owner.
 FakeNvs::reset();stored(100,100);writeAt("/shades.tmp",gpioModel(17,40,17,radio_proto::GP_Relay));restoreAllowed=true;
 {SomfyShadeController live;live.modelLoad.bootResult(ModelLoadStatus::loaded);ShadeConfigFile file;restore_options_t opts;opts.shades=true;assert(file.restoreFile(&live,"/shades.tmp",opts,RestoreOwner,RadioInstallToken{1}).complete);}
 // One actual ConfigFile + actual Stage + checked namespace-writer flow,
 // including selected preflight and model-file failure before any settings write.
 for(unsigned failure=0;failure<5;++failure){
  FakeNvs::reset();settingsDurability()=SettingsDurability{};stored(100,100);stored(101,900);writeAt("/shades.tmp",fullSettingsModel());restoreAllowed=true;
  const auto previous=settings;qualifySettings(settings);strcpy(settings.hostname,"ESPSomfyRTS");strcpy(settings.MQTT.username,"keep-user");strcpy(settings.MQTT.password,"keep-secret");
  if(failure==2)settings.MQTT.loadStatus.state=SettingsLoadState::failed;
  if(failure==4)settings.WIFI.loadStatus.state=SettingsLoadState::failed;
  SomfyShadeController live;live.modelLoad.bootResult(ModelLoadStatus::loaded);live.saveOK=failure!=1;radioApplyPartial=failure==3;
  ShadeConfigFile file;restore_options_t opts;opts.shades=opts.repeaters=opts.settings=opts.network=opts.mqtt=opts.transceiver=true;
  const auto effects=settingEffects;const auto result=file.restoreFile(&live,"/shades.tmp",opts,RestoreOwner,RadioInstallToken{1});
  if(failure==2){assert(!result.complete&&!result.modelApplied&&!result.counterWriteAttempted&&!FakeNvs::writeOpens&&!live.adopted&&live.modelLoad.publishable());}
  else if(failure==1){assert(!result.complete&&result.modelApplied&&!result.modelFile.saved()&&!FakeNvs::values.count("CFG/hostname")&&settingEffects==effects&&!live.gpio);}
  else if(failure==3){assert(!result.complete&&result.partial()&&result.modelFile.saved()&&result.settings.persistence.liveApplied&&strcmp(settings.hostname,"ESPSomfyRTS")==0&&settingEffects==effects&&!settingsDurability().status().restartAllowed());}
  else{assert(result.complete&&result.modelApplied&&result.modelFile.saved()&&result.settings.saved()&&strcmp(settings.hostname,"restored")==0&&settingEffects==effects+3);
   assert(strcmp(settings.MQTT.username,"keep-user")==0&&strcmp(settings.MQTT.password,"keep-secret")==0);
   assert(!FakeNvs::values.count("WIFI/ssid")&&!FakeNvs::values.count("SEC/type"));
   if(failure==4)assert(!settings.WIFI.loadStatus.usable()&&!settings.settingsUsable());
  }
  settings=previous;radioApplyPartial=false;
 }
 // Actual behavior administration query refuses movement, unresolved held
 // input and GPIO pulses before counter I/O, and preserves accepted STOP work.
 for(unsigned busy=0;busy<15;++busy){
  FakeNvs::reset();stored(100,100);stored(101,900);writeAt("/shades.tmp",aliasesModel(false));restoreAllowed=true;
  SomfyShadeController live;live.modelLoad.bootResult(ModelLoadStatus::loaded);live.shades[0].setShadeId(7);live.shades[0].setRemoteAddress(777);
  if(busy==0)live.shades[0].behaviour.onGroupCommand(somfy_commands::Down,1,0);
  if(busy==1){live.shades[0].frame.await=100;live.shades[0].frame.processed=false;}
  if(busy==2)live.shades[0].gpioRelease=100;
  if(busy==3){live.groups[0].setGroupId(1);live.groups[0].direction=1;live.groups[0].linkedShades[0]=99;}
  if(busy==4)loopQueue.queued=1;
  if(busy==5)live.transceiver.activityKnown=false;
  if(busy==6)live.transceiver.activity.schedulerIdle=false;
  if(busy==7)live.transceiver.activity.receivingFrame=true;
  if(busy==8)live.transceiver.activity.receivedFrames=1;
  if(busy==9)live.transceiver.activity.repeaterFrames=1;
  if(busy==10)live.transceiver.activity.scanning=true;
  if(busy==11)live.transceiver.activity.receiverPaused=true;
  if(busy==12)live.transceiver.activity.counterBusy=true;
  if(busy==13)live.transceiver.activity.txSealed=true;
  if(busy==14)live.transceiver.activity.suspended=true;
  ShadeConfigFile file;assert(file.begin("/shades.tmp",true));restore_options_t opts;opts.shades=opts.repeaters=true;
  assert(!file.restoreFile(&live,"/shades.tmp",opts,RestoreOwner,RadioInstallToken{1}).complete&&live.modelLoad.publishable()&&!live.adopted&&!live.gpio&&!live.commits);
  assert(FakeNvs::opens==0&&live.shades[0].getRemoteAddress()==777&&live.shades[0].effects.sends==0);
  assert(loopQueue.queued==unsigned(busy==4));loopQueue.queued=0;file.end();
 }
 restoreAllowed=true;FakeNvs::reset();unlink((LittleFS.root+"/shades.tmp").c_str());

 unlink((LittleFS.root+"/shades.cfg").c_str());rmdir(dir);puts("Actual ConfigFile boot loading: mounted/absent/full load/truncated header/repeater/trailing records pass before adoption");
}

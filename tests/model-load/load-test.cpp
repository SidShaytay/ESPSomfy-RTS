#define HAS_ETHERNET 0
#include "Arduino.h"
#include "ConfigRecords.h"
#include "ConfigValidation.h"
#include "ModelLoadSnapshot.h"
#include "ModelLoadTrust.h"
#include "ModelCommitResult.h"
#include <cassert>
#include <string>
#include <unistd.h>
#include <cstdio>
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
struct DummyFile {explicit operator bool()const{return false;}bool isDirectory(){return false;}void close(){}void flush(){}uint32_t position(){return 0;}};
struct {std::string root;const char *mountpoint(){return root.c_str();}DummyFile open(const char *,const char *){return {};}} LittleFS;
struct Preferences {};
struct restore_options_t{bool settings=false,shades=false,network=false,transceiver=false,repeaters=false,mqtt=false;};
struct GitUpdateOwnerToken{};
struct {bool allowed=true;bool transferRestoreClaimed(const GitUpdateOwnerToken &,uint64_t){return allowed;}}gitUpdate;
int64_t esp_timer_get_time(){return 1000000;}
void invalidateDeviceSettingsRevisions(){}
struct transceiver_config_t{uint8_t type=56;};
static unsigned settingEffects=0;
struct SettingsSave{bool saved()const{return true;}};
struct SettingSection{SettingsSave save(){++settingEffects;return {};}void apply(){++settingEffects;}};
struct {struct{unsigned major=4,minor=0,build=2;}appVersion;char serverId[10]{};SettingSection NTP,IP,WIFI,MQTT;SettingsSave save(){++settingEffects;return {};}}settings;
static settings_record_t settingsRecord(){return {};}
static net_record_t netRecord(){return {};}
template<class T>static trans_record_t transRecord(const T &){return {};}
static void applySettingsRecord(const settings_record_t &){++settingEffects;}
static void applyNetRecord(const net_record_t &,restore_options_t &,bool){++settingEffects;}
static void applyTransRecord(const trans_record_t &,transceiver_config_t &){++settingEffects;}
struct SomfyShadeController{
 unsigned adopted=0,repeatersRead=0,commits=0,gpio=0;
 struct Shade{uint8_t bitLength=0;uint8_t getShadeId(){return 255;}}shades[SOMFY_MAX_SHADES];
 bool begin();uint32_t repeaters[SOMFY_MAX_REPEATERS]{};
 ModelLoadTrust modelLoad;uint64_t configurationRevision=1,restoreRevision=1;bool saveOK=true;
 struct {unsigned starts=0;transceiver_config_t config;void begin(){++starts;}bool counterEditUncertain(){return false;}void setConfig(const transceiver_config_t &){++settingEffects;}}transceiver;
 ModelCommitResult commit(){assert(modelLoad.mayCommit());++commits;return {saveOK?ModelCommitState::saved:ModelCommitState::failed};}
 void configureGPIOs(){assert(modelLoad.usable());++gpio;}
};
using SomfyShade=SomfyShadeController::Shade;
SomfyShadeController somfy;
struct ConfigFile:ConfigReader {
 DummyFile file;String *_ramBuf=nullptr;bool readOnly=false,_writeFailed=false,_opened=false;
 ModelLoadSnapshot snapshot;ModelLoadStatus snapshotStatus=ModelLoadStatus::unknown;size_t readPosition=0;bool readFailed=false;
 config_header_t header;
 bool begin(const char *,bool=false);void end();bool isOpen(){return _opened;}
 bool readByte(uint8_t &)override;uint32_t position()override;bool seekRead(uint32_t);size_t readSize()const{return snapshot.size();}bool readHeader();
};
struct ShadeConfigFile:ConfigFile {
 using ConfigFile::begin;
 bool restoreFile(SomfyShadeController *,const char *,restore_options_t &,const GitUpdateOwnerToken &);
 bool validate();bool loadFile(SomfyShadeController *,const char *filename="/shades.cfg");
 static ModelLoadStatus loadBoot(SomfyShadeController *);
 void readModel(SomfyShadeController *s){++s->adopted;for(unsigned i=0;i<header.roomRecords;++i){room_record_t r;assert(readRoomRecord(*this,r));}for(unsigned i=0;i<header.shadeRecords;++i){shade_record_t r;assert(readShadeRecord(*this,header,r));}for(unsigned i=0;i<header.groupRecords;++i){group_record_t r;assert(readGroupRecord(*this,header,r));}}
 void readRepeaters(SomfyShadeController *s){++s->repeatersRead;uint32_t r[SOMFY_MAX_REPEATERS];assert(readRepeaterRecord(*this,r));}
};
#include "production-load.inc"
struct Writer:ConfigWriter {std::string bytes;bool writeBytes(const uint8_t *p,size_t n)override{bytes.append(reinterpret_cast<const char *>(p),n);return true;}};
static std::string validModel(){Writer w;config_header_t h;h.version=SHADE_HDR_VER;h.length=SHADE_HDR_SIZE;h.shadeRecordSize=SHADE_REC_SIZE;h.groupRecordSize=GROUP_REC_SIZE;h.roomRecordSize=ROOM_REC_SIZE;h.roomRecords=h.shadeRecords=h.groupRecords=0;h.repeaterRecords=1;h.repeaterRecordSize=REPEATER_REC_SIZE;h.settingsRecordSize=h.netRecordSize=h.transRecordSize=0;assert(writeConfigHeader(w,h));uint32_t repeaters[SOMFY_MAX_REPEATERS]{};assert(writeRepeaterRecord(w,repeaters));return w.bytes;}
static void write(const std::string &bytes){FILE *f=fopen((LittleFS.root+"/shades.cfg").c_str(),"wb");assert(f);assert(fwrite(bytes.data(),1,bytes.size(),f)==bytes.size());assert(fclose(f)==0);}
int main(){char dir[]="/tmp/espsomfy-model-load.XXXXXX";assert(mkdtemp(dir));LittleFS.root=dir;SomfyShadeController model;
 noteModelFilesystemMounted(false);assert(ShadeConfigFile::loadBoot(&model)==ModelLoadStatus::unavailable&&!model.adopted);
 noteModelFilesystemMounted(true);SomfyShadeController empty;assert(empty.begin()&&empty.modelLoad.usable()&&empty.gpio==1&&!empty.commits);assert(ShadeConfigFile::loadBoot(&model)==ModelLoadStatus::absent&&!model.adopted);
 const auto valid=validModel();write(valid);assert(ShadeConfigFile::loadBoot(&model)==ModelLoadStatus::loaded&&model.adopted==1&&model.repeatersRead==1);
 model.adopted=model.repeatersRead=0;
 write(valid.substr(0,valid.size()-1));assert(ShadeConfigFile::loadBoot(&model)==ModelLoadStatus::unavailable&&!model.adopted&&!model.repeatersRead);
 write(valid+"unexpected\n");assert(ShadeConfigFile::loadBoot(&model)==ModelLoadStatus::unavailable&&!model.adopted);
 write(valid.substr(0,3));SomfyShadeController failed;assert(!failed.begin()&&!failed.modelLoad.usable()&&!failed.gpio&&!failed.commits&&failed.transceiver.starts==1);assert(ShadeConfigFile::loadBoot(&model)==ModelLoadStatus::unavailable&&!model.adopted);
 write(valid);
 for(unsigned mode=0;mode<3;++mode){SomfyShadeController bad;bad.modelLoad.fail();ShadeConfigFile file;assert(file.begin("/shades.cfg",true));restore_options_t opts;opts.shades=mode==1;opts.repeaters=mode==2;
   assert(!file.restoreFile(&bad,"/shades.cfg",opts,{}));assert(!bad.adopted&&!bad.commits&&!bad.gpio&&!bad.modelLoad.usable());file.end();}
 for(bool success:{false,true}){SomfyShadeController bad;bad.modelLoad.fail();bad.saveOK=success;ShadeConfigFile file;assert(file.begin("/shades.cfg",true));restore_options_t opts;opts.shades=opts.repeaters=true;
   assert(file.restoreFile(&bad,"/shades.cfg",opts,{})==success);assert(bad.adopted==1&&bad.commits==1);assert(bad.modelLoad.usable()==success);assert(bad.gpio==unsigned(success));file.end();}
 assert(!settingEffects);
 unlink((LittleFS.root+"/shades.cfg").c_str());rmdir(dir);puts("Actual ConfigFile boot loading: mounted/absent/full load/truncated header/repeater/trailing records pass before adoption");
}

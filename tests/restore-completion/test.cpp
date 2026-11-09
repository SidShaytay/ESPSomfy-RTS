#include "RestoreResult.h"
#include "SettingsDurability.h"
#include "ProtoSlots.h"
#include "ProtoFrame.h"
#include "ProtoModelResults.h"
#include <ArduinoJson.h>
#include <cassert>
#include <string>
#include <vector>
#define PROTO_SOCKET_H
#define MQTT_CONNECTION_H
#define ESP_LOGE(...) ((void)0)
static proto_slot_t slots[PROTO_MAX_CLIENTS];
static char frameBuffer[PROTO_MAX_FRAME];
static BufferedJsonFormatter eventJson;
static bool loopAllowed=true;
static bool onLoopTask(){return loopAllowed;}
struct Wire {std::vector<uint32_t> closed;void close(uint32_t id){closed.push_back(id);}bool drop=false;std::vector<std::pair<uint32_t,std::string>> messages;bool text(uint32_t id,const char *bytes,size_t count){if(drop)return false;messages.emplace_back(id,std::string(bytes,count));return true;}}ws;
class ProtoSocket {public:bool hasClients()const;bool sending()const;void retireModelSessions();void modelPersistence(ModelCommitReport,uint32_t=0);}protoSocket;
struct MqttConnection {bool suspended=true;uint64_t lastConnect=99;unsigned disconnects=0;bool disconnect(){++disconnects;return true;}}mqtt;
#include "socket.inc"
#include "../../src/RestoreCompletion.cpp"
struct Source:SyncSource {
 bool room(uint8_t,room_snapshot_t&)override{return false;}
 bool shade(uint8_t,shade_snapshot_t&)override{return false;}
 bool group(uint8_t,group_snapshot_t&)override{return false;}
 bool repeater(uint8_t,uint32_t&)override{return false;}
 void header(ProtoWriter&)override{}void status(ProtoWriter&)override{}
};
static void clear(){for(auto &slot:slots)freeSlot(slot);ws.messages.clear();ws.drop=false;mqtt.disconnects=0;mqtt.lastConnect=99;}
int main(){
 clear();for(uint32_t id:{11,22}){auto *slot=claimSlot(slots,PROTO_MAX_CLIENTS,id,false);assert(slot);slot->loopId=id;slot->cur.section=sync_section_t::shades;slot->cur.index=17;slot->awaitingNext=true;}
 RestoreApplyResult result;result.modelFile={ModelCommitState::saved};
 completeRestoreModel(result);assert(ws.messages.empty()&&!slots[0].wants.load()&&!mqtt.disconnects);
 result.modelApplied=true;result.modelFile={ModelCommitState::failed,ModelCommitStage::write};
 completeRestoreModel(result);assert(ws.messages.size()==2&&ws.messages[0].first==11&&ws.messages[1].first==22&&mqtt.disconnects==1&&mqtt.lastConnect==0&&mqtt.suspended);
 for(auto &message:ws.messages){JsonDocument doc;assert(!deserializeJson(doc,message.second));assert(doc["t"]==static_cast<unsigned>(proto_type_t::modelPersistence));assert(doc["d"][protoField(proto_field_t::modelSaveState).key]==int(ModelCommitState::failed));assert(!doc["d"][protoField(proto_field_t::durable).key].as<bool>());}
 for(unsigned i=0;i<2;++i){assert(!clientMayMutate(slots,PROTO_MAX_CLIENTS,slots[i].clientId.load()));slots[i].wants.fetch_or(WANTS_NEXT|WANTS_SYNC_AGAIN);nextSyncStep(slots[i],100);assert(!clientMayMutate(slots,PROTO_MAX_CLIENTS,slots[i].clientId.load()));}
 assert(ws.closed.size()==2&&!slots[2].wants.load());
 // Returned applied MQTT proof includes the deferred hook: do not duplicate it.
 ws.messages.clear();mqtt.disconnects=0;mqtt.lastConnect=99;
 result.modelFile={ModelCommitState::saved};
 result.settings.state=RestoreSettingsState::saved;result.settings.requestedNamespaces=result.settings.verifiedNamespaces=result.settings.appliedNamespaces=settingsNamespaceBit(SettingsNamespace::mqtt);
 completeRestoreModel(result);assert(ws.messages.size()==2&&!mqtt.disconnects&&mqtt.lastConnect==99);
 for(const auto &message:ws.messages){JsonDocument doc;assert(!deserializeJson(doc,message.second));assert(doc["d"][protoField(proto_field_t::durable).key].as<bool>());assert(doc["d"][protoField(proto_field_t::modelSaveState).key].as<int>()==int(ModelCommitState::saved));}
 // Verified/requested-only or partial persistence is not that proof.
 result.settings.state=RestoreSettingsState::partial;result.settings.persistence={settings_save_state_t::partial};
 completeRestoreModel(result);assert(mqtt.disconnects==1&&mqtt.lastConnect==0);
 // Transport failure cannot prevent reducing-authority broker cleanup.
 ws.drop=true;const auto calls=mqtt.disconnects;completeRestoreModel(result);assert(mqtt.disconnects==calls+1);
 clear();loopAllowed=false;protoSocket.retireModelSessions();assert(!slots[0].wants.load());
}

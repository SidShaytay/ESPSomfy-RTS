#include "LoopQueue.h"
#include "ProtoSlots.h"
#include "SomfyEditor.h"
#include "DeviceOps.h"
#include "ProtoOps.h"
#include "ModelLoadTrust.h"
#include "TxBuffer.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <assert.h>
#include <functional>
#include <vector>
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
static proto_slot_t slots[PROTO_MAX_CLIENTS];
static bool onLoopTask(){return true;}
static unsigned editsFreed=0,savesFreed=0,requests=0,edits=0,devices=0,notes=0,rawFrames=0;
static std::function<void()> duringParse,duringSend;
static bool rawAccepted=true,badParse=false;
void cancelModelEdit(model_edit_t *p){++editsFreed;delete p;}
void freeSettingsSave(settings_save_t *p){++savesFreed;delete p;}
struct Wire{std::vector<uint32_t> closed;void close(uint32_t id){closed.push_back(id);}}ws;
struct ProtoSocket {
 bool acceptsClientMutation(uint32_t)const;bool acceptsMutation(const loop_item_t &)const;
 void discardRetiredMutation(const loop_item_t &);void retireModelSessions();
 void replyRefused(const proto_reply_to_t &,proto_error_t,const char*){}
 void replyRequest(const proto_reply_to_t &,const shade_request_t &,shade_request_result_t){}
 void replyRead(const proto_reply_to_t &,uint8_t,uint8_t){}
}protoSocket;
bool counterCommandAllowed(uint32_t,somfy_commands){return true;}
struct SomfyShadeController {
 ModelLoadTrust modelLoad{ModelLoadStatus::loaded};
 struct {
  somfy_tx_job_t accepted;
  bool txSuspended(){return false;}bool txHasRoom(){return true;}
  uint32_t submitTx(const somfy_tx_job_t &job,bool,uint16_t){if(duringSend)duringSend();if(!rawAccepted)return 0;accepted=job;++rawFrames;return 9;}
 }transceiver;
 uint32_t sendFrame(somfy_frame_t &,uint8_t,uint16_t=0,bool=false);
}somfy;
struct {shade_request_result_t run(const shade_request_t&){++requests;return shade_request_result_t::done;}}shadeRequests;
void noteRestartOperationAccepted(){++notes;}
void runModelEdit(model_edit_t *p){++edits;cancelModelEdit(p);}
void runDeviceOp(const loop_item_t &item){++devices;if(item.save)freeSettingsSave(item.save);}
void runModelCommit(const loop_item_t &){}
void runOwnedRestore(const GitUpdateOwnerToken &,uint8_t){}
LoopQueue loopQueue;
struct AsyncWebSocketClient{uint32_t number;uint32_t id(){return number;}};
static unsigned errors=0,oks=0;
void replyError(AsyncWebSocketClient*,uint32_t,proto_error_t,const char*){++errors;}
void replyOk(AsyncWebSocketClient*,uint32_t){++oks;}
const char *parseRemoteCommandOp(JsonObjectConst,somfy_frame_t &frame,uint8_t &repeat,proto_error_buffer_t &){
 frame.remoteAddress=123;frame.rollingCode=42;frame.cmd=somfy_commands::Down;frame.bitLength=56;repeat=1;if(duringParse)duringParse();return badParse?"invalid":nullptr;
}
#include "controller.inc"
#include "socket.inc"
#include "queue.inc"
static loop_item_t command(uint32_t id){loop_item_t item;item.replyTo={id,1};return item;}
int main(){
 loopQueue.begin();auto *old=claimSlot(slots,PROTO_MAX_CLIENTS,71,false);assert(old);
 auto item=command(71);assert(loopQueue.sealForRestart());protoSocket.retireModelSessions();loopQueue.reopenAfterRefusedRestart();
 assert(!loopQueue.submit(item)&&loopQueue.pending()==0&&notes==0);
 old->wants.fetch_or(WANTS_NEXT|WANTS_SYNC_AGAIN);nextSyncStep(*old,10);assert(!loopQueue.submit(item));
 freeSlot(*old);auto *fresh=claimSlot(slots,PROTO_MAX_CLIENTS,72,false);assert(fresh);retireSlot(*old,71);
 assert(loopQueue.submit(command(72)));loopQueue.drain();assert(requests==1&&notes==1);
 // Recheck after acceptance; disconnected/retired sessions lose queued authority.
 auto *edit=new model_edit_t;edit->replyTo={72,2};loop_item_t editing;editing.type=loop_item_type_t::model_edit;editing.edit=edit;
 assert(loopQueue.submit(editing));auto *save=new settings_save_t;loop_item_t saving;saving.type=loop_item_type_t::proto_device;saving.op=uint8_t(proto_type_t::saveSettings);saving.replyTo={72,3};saving.save=save;
 assert(loopQueue.submit(saving));protoSocket.retireModelSessions();loopQueue.drain();assert(!edits&&!devices&&editsFreed==1&&savesFreed==1);
 // A rejected submit retains caller ownership, matching the real wrappers.
 edit=new model_edit_t;edit->replyTo={72,4};editing.edit=edit;assert(!loopQueue.submit(editing));cancelModelEdit(edit);assert(editsFreed==2);
 save=new settings_save_t;saving.save=save;assert(!loopQueue.submit(saving));freeSettingsSave(save);assert(savesFreed==2);
 // A delayed parser cannot send raw RF after retirement and queue reopening.
 freeSlot(*fresh);claimSlot(slots,PROTO_MAX_CLIENTS,73,false);AsyncWebSocketClient client{73};proto_message_t message;message.id=9;
 duringParse=[]{assert(loopQueue.sealForRestart());protoSocket.retireModelSessions();loopQueue.reopenAfterRefusedRestart();};
 const auto before=notes;JsonDocument payload;sendRemoteCommand(&client,message,payload.as<JsonObjectConst>());
 assert(!rawFrames&&notes==before&&errors==1);duringParse={};
 // Producer ownership spans the scheduler submission, so restore cannot overtake it.
 freeSlot(slots[0]);claimSlot(slots,PROTO_MAX_CLIENTS,74,false);client.number=74;
 duringSend=[]{assert(!loopQueue.sealForRestart());};sendRemoteCommand(&client,message,payload.as<JsonObjectConst>());
 assert(rawFrames==1&&notes==before+1&&oks==1&&somfy.transceiver.accepted.frame.remoteAddress==123&&somfy.transceiver.accepted.frame.rollingCode==42);duringSend={};
 rawAccepted=false;sendRemoteCommand(&client,message,payload.as<JsonObjectConst>());assert(notes==before+1&&loopQueue.sealForRestart());loopQueue.reopenAfterRefusedRestart();
 badParse=true;sendRemoteCommand(&client,message,payload.as<JsonObjectConst>());assert(loopQueue.sealForRestart());
}

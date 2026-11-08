#pragma once
// Compile the actual LoopQueue consumer with fake RTOS/storage/other domains.
// Only the local adapter and typed request sink under review can issue requests.
#define configfile_h
#define configsettings_h
#define CONFIG_HEADER_H
#define CONFIG_RECORDS_H
#define SOMFY_EDITOR_H
#define PROTO_SOCKET_H
#define DEVICE_OPS_H
#define utils_h
#include "LoopQueue.h"
struct restore_options_t {bool settings=false,shades=false,network=false,transceiver=false,repeaters=false,mqtt=false;};
struct rebootDelay_t {bool reboot=false;int rebootTime=0;bool closed=false;};
inline rebootDelay_t rebootDelay;
struct ShadeConfigFile {static bool restore(SomfyShadeController *,const char *,restore_options_t &) {return false;}};
inline void (*modelEditHook)()=nullptr;
inline void runModelEdit(model_edit_t *) {if(modelEditHook)modelEditHook();}
inline void runDeviceOp(const loop_item_t &) {}
inline unsigned loopReads=0;
struct ProtoSocket {
  void replyRequest(const proto_reply_to_t &,const shade_request_t &,shade_request_result_t) {}
  void replyRead(const proto_reply_to_t &,uint8_t,uint8_t) {++loopReads;}
};
inline ProtoSocket protoSocket;
#define ShadeRequests RecordingRequests
#include "../../src/LoopQueue.cpp"
#undef ShadeRequests
inline LoopQueue loopQueue;

#pragma once
// Compile the actual LoopQueue consumer with fake RTOS/storage/other domains.
// Only the local adapter and typed request sink under review can issue requests.
#define configfile_h
#ifndef configsettings_h
#define configsettings_h
#define LOCAL_QUEUE_FAKE_RESTORE_OPTIONS
#endif
#define CONFIG_HEADER_H
#define CONFIG_RECORDS_H
#define SOMFY_EDITOR_H
#define PROTO_SOCKET_H
#define DEVICE_OPS_H
#include "LoopQueue.h"
#include "GitUpdate.h"
#include "SafeRestart.h"
#include "RestoreExecution.h"
#ifndef LOCAL_QUEUE_REAL_RESTORE_EXECUTION
inline void runOwnedRestore(const GitUpdateOwnerToken &,uint8_t){}
#endif
#ifdef LOCAL_QUEUE_FAKE_RESTORE_OPTIONS
struct restore_options_t {bool settings=false,shades=false,network=false,transceiver=false,repeaters=false,mqtt=false;};
#endif
#ifndef LOCAL_QUEUE_EXTERNAL_REBOOT
inline bool pendingRestart=false;
bool restartPending(){return pendingRestart;}
inline unsigned restartMutationNotes=0;
inline void noteRestartOperationAccepted(){++restartMutationNotes;}
inline RestartResult requestOwnedRestart(const GitUpdateOwnerToken &,uint32_t){assert(false);return {};}
#endif
#ifndef LOCAL_QUEUE_REAL_UPDATE_OWNER
inline GitUpdateClass gitUpdate;
inline bool GitUpdateClass::claimTransfer(const GitUpdateOwnerToken &,uint64_t,bool){return false;}
inline bool GitUpdateClass::transferResource(const GitUpdateOwnerToken &,bool){assert(false);return false;}
inline bool GitUpdateClass::transferContinuing(const GitUpdateOwnerToken &,uint64_t){assert(false);return false;}
inline bool GitUpdateClass::holdTransferRestart(const GitUpdateOwnerToken &){assert(false);return false;}
inline bool GitUpdateClass::finishTransfer(const GitUpdateOwnerToken &,uint64_t,bool){assert(false);return false;}
inline bool GitUpdateClass::closeTransfer(const GitUpdateOwnerToken &){assert(false);return false;}
inline bool GitUpdateClass::releaseExternal(const GitUpdateOwnerToken &){assert(false);return false;}
#endif
#ifndef LOCAL_QUEUE_EXTERNAL_CONFIG_FILE
struct ShadeConfigFile {static bool restore(SomfyShadeController *,const char *,restore_options_t &,const GitUpdateOwnerToken &) {return false;}};
#endif
inline void (*modelEditHook)()=nullptr;
inline void runModelEdit(model_edit_t *) {if(modelEditHook)modelEditHook();}
inline void runDeviceOp(const loop_item_t &) {}
inline void runModelCommit(const loop_item_t &) {}
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

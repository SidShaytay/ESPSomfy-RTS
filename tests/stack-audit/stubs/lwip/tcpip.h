#pragma once
#include "sdk-stubs.h"
constexpr int ERR_OK=0;
inline int tcpip_try_callback(void (*callback)(void*),void*){
 ++AuditSdkStubs::tcpipPosts;
 if(AuditSdkStubs::tcpipResult==ERR_OK)AuditSdkStubs::tcpipCallback=callback;
 return AuditSdkStubs::tcpipResult;
}

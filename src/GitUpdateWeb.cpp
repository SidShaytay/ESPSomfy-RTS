#include "GitUpdateWeb.h"
#include <stdio.h>
#include "GitUpdate.h"
#include "GitUpdateController.h"
#include "GitUpdateBootHealth.h"
#include "GitUpdateAdvice.h"
#include "ProtoDeviceArgs.h"
#include "outputs/ProtoSocket.h"
namespace GitUpdateWeb {
namespace {
void refuse(const loop_item_t &item,GitUpdateRequest result) {
  switch(result) {
    case GitUpdateRequest::busy:
      protoSocket.replyRefused(item.replyTo,proto_error_t::busy,"The updater or controller is busy. Wait for current work to finish.");return;
    case GitUpdateRequest::invalidCandidate:
      protoSocket.replyRefused(item.replyTo,proto_error_t::badRequest,"The release changed since it was checked. Check again before installing.");return;
    default:
      protoSocket.replyRefused(item.replyTo,proto_error_t::busy,"GitHub updates are not available on this controller right now.");return;
  }
}
void decimal(char (&out)[21],uint64_t value){snprintf(out,sizeof(out),"%llu",(unsigned long long)value);}
}
void runProtocol(const loop_item_t &item) {
  const auto &args=item.device;
  uint64_t operation=0;
  auto result=GitUpdateRequest::accepted;
  switch(args.action) {
    case ProtoDeviceArgs::updateCheck:result=requestBrowserGitUpdateCheck(operation);break;
    case ProtoDeviceArgs::updateInstall: {
      const auto current=gitUpdate.snapshot();
      result=args.id && args.id==current.candidate.generation ?
        requestBrowserGitUpdateInstall(current.candidate,operation) : GitUpdateRequest::invalidCandidate;
      break;
    }
    case ProtoDeviceArgs::updateCancel:
      if(!cancelBrowserGitUpdate(args.id)) {
        protoSocket.replyRefused(item.replyTo,proto_error_t::badRequest,"That update can no longer be cancelled from this browser.");return;
      }
      break;
    default:break;
  }
  if(result!=GitUpdateRequest::accepted){refuse(item,result);return;}
  const auto s=gitUpdate.snapshot();
  char text[21];
  auto reply=protoSocket.beginReply(item.replyTo);
  reply.add(proto_field_t::updateSupported,s.supported);
  reply.add(proto_field_t::updatePhase,gitUpdatePhase(s.phase));
  reply.add(proto_field_t::updateReason,gitUpdateAdvice(s.reason));
  reply.add(proto_field_t::updateReasonCode,uint8_t(s.reason));
  reply.add(proto_field_t::updateInstalled,s.installed);
  reply.add(proto_field_t::updateBusy,s.busy);
  reply.add(proto_field_t::updateCanInstall,s.canInstall && !s.manual);
  reply.add(proto_field_t::updateCancellable,s.cancellable && gitUpdateBrowserOwned(s.operation));
  reply.add(proto_field_t::updateOwned,gitUpdateBrowserOwned(s.operation));
  reply.add(proto_field_t::updateProgress,s.progress);
  reply.add(proto_field_t::updateRetries,s.retryCount);
  reply.add(proto_field_t::updateRetryMs,s.retryDelayMs);
  reply.add(proto_field_t::updateRemainingMs,s.installContinuation?s.installRemainingMs:0);
  reply.add(proto_field_t::updateBootPending,gitUpdateBootStatus().pending);
  if(s.operation){decimal(text,s.operation);reply.add(proto_field_t::updateOperation,text);}
  if(s.candidate.generation && !s.manual) {
    decimal(text,s.candidate.generation);reply.add(proto_field_t::updateCandidate,text);
    reply.add(proto_field_t::updateVersion,s.candidate.version);
    reply.add(proto_field_t::updateSize,s.candidate.size);
    reply.add(proto_field_t::updateImageSha,s.candidate.imageSha);
  }
  protoSocket.endReply(item.replyTo);
}
}

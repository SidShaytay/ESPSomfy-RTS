#pragma once
#include <cstddef>
#include <cstdint>

enum class ManualUploadResult { notStarted, failed, success };

// Keep the first failure's reboot deadline: additional refused chunks must
// never postpone recovery of a quiesced controller. All spans fit in int32_t.
class ManualUploadDeadline {
  uint32_t deadline=0;
  bool armed=false;
public:
  void begin(uint32_t now) { deadline=now+180000;armed=true; }
  uint32_t shorten(uint32_t now) {
    const uint32_t soon=now+500;
    if(!armed || static_cast<int32_t>(deadline-soon)>0) { deadline=soon;armed=true; }
    return deadline;
  }
  uint32_t at() const { return deadline; }
  bool expired(uint32_t now) const { return armed && static_cast<int32_t>(now-deadline)>=0; }
};

// File-final only means the multipart part ended. Activation is deferred until
// the whole authenticated request has ended and exactly one part was accepted.
// Once servicing has been quiesced, every outcome retains the gate and reboots.
template<class Request,class Writer,class Effects>
void failManualUpload(Request *request,Writer &writer,Effects &effects) {
  if(!request->getAttribute("manualOwned",false)) return;
  request->setAttribute("manualInvalid",true);
  if(writer.isRunning()) writer.abort();
  effects.reboot();
}

template<class Request,class Gate,class Writer,class Effects>
bool beginManualUpload(Request *request,Gate &gate,Writer &writer,Effects &effects,int command) {
  if(request->getAttribute("manualAttempted",false)) {
    // Preserve this request's ownership: a second file must abort its first
    // image, never masquerade as a different refused request.
    failManualUpload(request,writer,effects);
    return false;
  }
  request->setAttribute("manualAttempted",true);
  if(!gate.beginManualUpdate()) return false;
  if(!writer.begin(command)) { gate.endManualUpdate();return false; }
  request->setAttribute("manualOwned",true);
  effects.quiesce();
  request->onDisconnect([request,&writer,&effects]() {
    if(!request->getAttribute("manualOwned",false)) return;
    if(!request->getAttribute("manualActivated",false) && writer.isRunning()) writer.abort();
    effects.reboot();
  });
  return true;
}

template<class Request,class Writer,class Effects>
bool writeManualUpload(Request *request,Writer &writer,Effects &effects,uint8_t *data,size_t size,bool final) {
  if(!request->getAttribute("manualOwned",false)) return false;
  if(request->getAttribute("manualInvalid",false) || request->getAttribute("manualComplete",false) || effects.expired()) {
    failManualUpload(request,writer,effects);return false;
  }
  if(size) {
    if(!data || writer.write(data,size)!=size) { failManualUpload(request,writer,effects);return false; }
    request->setAttribute("manualData",true);
  }
  if(final) {
    if(!request->getAttribute("manualData",false)) { failManualUpload(request,writer,effects);return false; }
    request->setAttribute("manualComplete",true);
  }
  return true;
}

template<class Request,class Writer,class Effects>
ManualUploadResult finishManualUpload(Request *request,Writer &writer,Effects &effects) {
  if(!request->getAttribute("manualOwned",false)) return ManualUploadResult::notStarted;
  if(request->getAttribute("manualActivated",false)) return ManualUploadResult::success;
  if(request->getAttribute("manualInvalid",false) || !request->getAttribute("manualComplete",false) || writer.hasError() || effects.expired()) {
    failManualUpload(request,writer,effects);return ManualUploadResult::failed;
  }
  if(!writer.end()) { failManualUpload(request,writer,effects);return ManualUploadResult::failed; }
  request->setAttribute("manualActivated",true);
  effects.reboot();
  return ManualUploadResult::success;
}

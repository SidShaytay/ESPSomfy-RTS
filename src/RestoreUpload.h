#pragma once
// One restore owns its temporary file and the update gate from the first
// chunk through validation and queued restore. Request attributes live until
// its disconnect callback runs. Templates keep this lifecycle host-testable.
template<class Request, class Gate, class Files>
void discardRestoreUpload(Request *request, Gate &gate, Files &files) {
  if(!request->getAttribute("restoreOwned", false) || request->getAttribute("restoreQueued", false)) return;
  request->setAttribute("restoreOwned", false);
  request->setAttribute("restoreComplete", false);
  files.remove("/shades.tmp");
  gate.endManualUpdate();
}
template<class Request, class Gate, class Files>
bool beginRestoreUpload(Request *request, Gate &gate, Files &files) {
  // Multipart requests can contain several files. Never combine one upload's
  // validation with another file or let a second file overwrite its temp.
  if(request->getAttribute("restoreAttempted", false)) {
    discardRestoreUpload(request, gate, files);
    return false;
  }
  request->setAttribute("restoreAttempted", true);
  if(!gate.beginManualUpdate()) return false;
  request->setAttribute("restoreOwned", true);
  request->setAttribute("restoreComplete", false);
  request->setAttribute("restoreQueued", false);
  request->onDisconnect([request, &gate, &files]() { discardRestoreUpload(request, gate, files); });
  return true;
}
template<class Request>
void finishRestoreUpload(Request *request) {
  if(request->getAttribute("restoreOwned", false)) request->setAttribute("restoreComplete", true);
}
template<class Request>
bool restoreUploadComplete(Request *request) {
  return request->getAttribute("restoreOwned", false) && request->getAttribute("restoreComplete", false);
}
template<class Request>
void queueRestoreUpload(Request *request) { request->setAttribute("restoreQueued", true); }

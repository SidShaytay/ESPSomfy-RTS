#pragma once
// HTTP reports only upload/admission here. The loop reports persistence later.
struct ApplicationUploadResponse {
  int status;
  const char *json;
};
inline ApplicationUploadResponse applicationUploadResponse(bool uploaded,
                                                           bool queued) {
  if (!uploaded)
    return {500,
            queued
                ? "{\"status\":\"ERROR\",\"desc\":\"Application upload failed. "
                  "Model save queued; restart manually after a successful "
                  "Commit.\"}"
                : "{\"status\":\"ERROR\",\"desc\":\"Application upload failed; "
                  "model save not queued. Retry Commit, then Restart.\"}"};
  if (!queued)
    return {503, "{\"status\":\"ERROR\",\"desc\":\"Application uploaded; model "
                 "save not queued. Retry Commit, then Restart.\"}"};
  return {202, "{\"status\":\"QUEUED\",\"desc\":\"Application uploaded; model "
               "save queued. Restart needs a verified model file and no pending operations.\"}"};
}

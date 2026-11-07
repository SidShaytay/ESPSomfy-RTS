#pragma once
// Snapshot read under the existing radio/TX locks and the receive ISR mux.
// Queued or partially received remote work must reach the model before its
// position/counter snapshot is persisted for an installation.
struct RadioUpdateActivity {
  bool schedulerIdle = false;
  bool suspended = false;
  unsigned receivedFrames = 0;
  bool receivingFrame = false;
  unsigned repeaterFrames = 0;
  bool scanning = false;
  bool canPause() const {
    return schedulerIdle && !suspended && !receivedFrames && !receivingFrame &&
      !repeaterFrames && !scanning;
  }
};

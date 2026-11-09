#pragma once
#include "TxBuffer.h"
#include "BitLength.h"
#include <string.h>
// Only needed while an explicit reservation/quarantine exists. Decode a copy:
// raw repeater metadata deliberately does not carry a synthesized RF address.
inline bool decodeCounterRepeater(const somfy_tx_job_t &job,somfy_frame_t &frame) {
  if(!supportedBitLength(job.tx.bit_length))return false;
  byte bytes[10];memcpy(bytes,job.tx.payload,sizeof(bytes));
  frame={};frame.bitLength=job.tx.bit_length;frame.decodeFrame(bytes);
  return frame.valid;
}
inline bool counterTxBlocked(uint32_t blocked,const somfy_tx_job_t &job) {
  if(!blocked)return false;
  if(!job.repeater)return job.frame.remoteAddress==blocked;
  somfy_frame_t frame;
  return !decodeCounterRepeater(job,frame) || frame.remoteAddress==blocked;
}

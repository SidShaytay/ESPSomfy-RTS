#pragma once
#include "ModelLoadSnapshot.h"
// Loop-owned. Unknown is deliberately not a trusted empty model. Only startup
// proof or a fully validated explicit model restore may establish trust.
class ModelLoadTrust {
  ModelLoadStatus value=ModelLoadStatus::unknown;
  bool recoveryWrite=false;
public:
  explicit ModelLoadTrust(ModelLoadStatus initial=ModelLoadStatus::unknown):value(initial){}
  ModelLoadStatus status()const{return value;}
  bool usable()const{return value==ModelLoadStatus::loaded||value==ModelLoadStatus::absent;}
  bool mayCommit()const{return usable()||recoveryWrite;}
  void fail(){value=ModelLoadStatus::unavailable;recoveryWrite=false;}
  void bootResult(ModelLoadStatus result){value=result;recoveryWrite=false;}
  // Only call after complete immutable input validation/adoption, never for
  // settings-only or partial-model restore. This does not authorize commands.
  void beginRecoveryCommit(){fail();recoveryWrite=true;}
  void finishRecoveryCommit(bool saved){recoveryWrite=false;value=saved?ModelLoadStatus::loaded:ModelLoadStatus::unavailable;}
};

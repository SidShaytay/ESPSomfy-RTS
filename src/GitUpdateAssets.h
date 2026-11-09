#pragma once
#include "GitUpdateAttestation.h"
// Hashes the immutable canonical firmware bundle; generic targets are unsupported.
UpdateAttestation gitUpdateAssetsStatus(UpdateAttestationControl control={});
bool gitUpdateAssetsMatch();

#pragma once
#include "GitUpdateAttestation.h"
UpdateAttestation gitUpdateBootloaderStatus(UpdateAttestationControl control={});

// Compare installed raw bootloader bytes with this build's generated pinned
// rollback-enabled bootloader. Cached: the updater never writes the bootloader.
bool gitUpdateBootloaderMatches();

// Required bootloader identity for authenticated release compatibility metadata.
const char *gitUpdateBootloaderDigest();

// Compiled address/size/digest record, also validated by offline packaging.
const char *gitUpdateBootloaderIdentity();

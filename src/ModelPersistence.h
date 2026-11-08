#pragma once
#include "ConfigRecords.h"
#include "ModelCommitResult.h"
#include <stdint.h>
#include <stdio.h>

// The model-owning loop must keep the source stable. Serialization may run
// three times and must not mutate the model, publish or access rolling-code
// NVS.
class ModelSerializer {
public:
  virtual ~ModelSerializer() = default;
  virtual bool write(ConfigWriter &writer) = 0;
};
// Focused VFS boundary: production uses the mounted LittleFS stdio functions;
// tests inject failures while exercising the same writer/verification sequence.
struct ModelFileOperations {
  FILE *(*open)(const char *, const char *);
  size_t (*write)(const void *, size_t, size_t, FILE *);
  size_t (*read)(void *, size_t, size_t, FILE *);
  int (*error)(FILE *);
  int (*flush)(FILE *);
  int (*sync)(int);
  int (*descriptor)(FILE *);
  int (*close)(FILE *);
  int (*replace)(const char *, const char *);
  int (*remove)(const char *);
};
const ModelFileOperations &modelFileOperations();
// Stage and verify before rename-over-target; never truncate the old file.
// After replacement, failed readback is uncertain and is never rolled back.
// Caller supplies LittleFS.mountpoint(); this does not mount or format a disk.
ModelCommitResult
commitModelFile(const char *mountpoint, ModelSerializer &source,
                const ModelFileOperations &io = modelFileOperations());

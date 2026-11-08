#include "ModelPersistence.h"
#include <string.h>
#include <unistd.h>
namespace {
const ModelFileOperations operations = {fopen, fwrite, fread,  ferror, fflush,
                                        fsync, fileno, fclose, rename, remove};
class StageWriter : public ConfigWriter {
  const ModelFileOperations &io;
  FILE *file;
  bool failed = false;

public:
  StageWriter(const ModelFileOperations &ops, FILE *f) : io(ops), file(f) {}
  bool writeBytes(const uint8_t *data, size_t length) override {
    if (failed)
      return false;
    if (length && io.write(data, 1, length, file) != length)
      failed = true;
    return !failed;
  }
  bool good() const { return !failed; }
};
class ComparingWriter : public ConfigWriter {
  const ModelFileOperations &io;
  FILE *file;
  ModelCommitStage failed = ModelCommitStage::none;

public:
  ComparingWriter(const ModelFileOperations &ops, FILE *f) : io(ops), file(f) {}
  bool writeBytes(const uint8_t *expected, size_t length) override {
    if (failed != ModelCommitStage::none)
      return false;
    uint8_t bytes[64];
    while (length) {
      const size_t count = length < sizeof(bytes) ? length : sizeof(bytes);
      if (io.read(bytes, 1, count, file) != count) {
        failed = io.error(file) ? ModelCommitStage::verifyRead
                                : ModelCommitStage::verifyBytes;
        return false;
      }
      if (memcmp(bytes, expected, count)) {
        failed = ModelCommitStage::verifyBytes;
        return false;
      }
      length -= count;
      expected += count;
    }
    return true;
  }
  ModelCommitStage finish() {
    if (failed == ModelCommitStage::none) {
      uint8_t extra;
      const auto count = io.read(&extra, 1, 1, file);
      if (io.error(file))
        failed = ModelCommitStage::verifyRead;
      else if (count)
        failed = ModelCommitStage::verifyBytes;
    }
    return failed;
  }
};
ModelCommitStage verify(const char *path, ModelSerializer &source,
                        const ModelFileOperations &io) {
  FILE *file = io.open(path, "rb");
  if (!file)
    return ModelCommitStage::verifyOpen;
  ComparingWriter writer(io, file);
  const bool serialized = source.write(writer);
  auto failed = writer.finish();
  if (!serialized && failed == ModelCommitStage::none)
    failed = ModelCommitStage::serialize;
  if (io.close(file) && failed == ModelCommitStage::none)
    failed = ModelCommitStage::verifyClose;
  return failed;
}
} // namespace
const ModelFileOperations &modelFileOperations() { return operations; }
ModelCommitResult commitModelFile(const char *mountpoint,
                                  ModelSerializer &source,
                                  const ModelFileOperations &io) {
  ModelCommitResult result;
  // Only fixed model filenames are accepted. Keep path memory bounded and
  // derive the VFS prefix from the actual filesystem mount point.
  constexpr size_t capacity = 128;
  char target[capacity], staging[capacity];
  size_t length = mountpoint ? strnlen(mountpoint, capacity) : 0;
  if (!length || length == capacity || mountpoint[0] != '/' || !io.open ||
      !io.write || !io.read || !io.error || !io.flush || !io.sync ||
      !io.descriptor || !io.close || !io.replace || !io.remove) {
    result.stage = ModelCommitStage::path;
    return result;
  }
  while (length > 1 && mountpoint[length - 1] == '/')
    --length;
  if (length == 1)
    --length;
  const int a = snprintf(target, sizeof(target), "%.*s/shades.cfg", int(length),
                         mountpoint);
  const int b = snprintf(staging, sizeof(staging), "%.*s/shades.cfg.new",
                         int(length), mountpoint);
  if (a < 0 || b < 0 || size_t(a) >= sizeof(target) ||
      size_t(b) >= sizeof(staging)) {
    result.stage = ModelCommitStage::path;
    return result;
  }
  FILE *file = io.open(staging, "wb");
  if (!file) {
    result.stage = ModelCommitStage::open;
    return result;
  }
  StageWriter writer(io, file);
  const bool serialized = source.write(writer);
  if (!writer.good() || io.error(file))
    result.stage = ModelCommitStage::write;
  else if (!serialized)
    result.stage = ModelCommitStage::serialize;
  // Close every opened handle, retaining the first failure. Even a final
  // buffered write, sync or close failure prevents replacement.
  if (io.flush(file) && result.stage == ModelCommitStage::none)
    result.stage = ModelCommitStage::flush;
  if (io.error(file) && result.stage == ModelCommitStage::none)
    result.stage = ModelCommitStage::write;
  const int fd = io.descriptor(file);
  if ((fd < 0 || io.sync(fd)) && result.stage == ModelCommitStage::none)
    result.stage = ModelCommitStage::sync;
  if (io.close(file) && result.stage == ModelCommitStage::none)
    result.stage = ModelCommitStage::close;
  if (result.stage == ModelCommitStage::none)
    result.stage = verify(staging, source, io);
  if (result.stage == ModelCommitStage::none) {
    if (io.replace(staging, target))
      result.stage = ModelCommitStage::replace;
    else {
      result.stage = verify(target, source, io);
      result.state = result.stage == ModelCommitStage::none
                         ? ModelCommitState::saved
                         : ModelCommitState::uncertain;
    }
  }
  if (!result.saved())
    io.remove(staging); // Best effort; never remove the canonical file.
  return result;
}

const char *modelCommitFailureDescription(ModelCommitState state) {
  return state == ModelCommitState::uncertain
             ? "Running changes applied; model file could not be verified. "
               "Retry Commit before restarting."
             : "Running changes applied; model file not saved. Previous file "
               "retained. Retry Commit before restarting.";
}

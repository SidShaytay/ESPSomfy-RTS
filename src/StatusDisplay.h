#pragma once
#include <stdint.h>

// Read-only values only: no controller, command queue or credentials cross this boundary.
namespace StatusDisplay {
enum class Link { disconnected, connecting, connected, accessPoint };
enum class Web { starting, listening, error };
enum class Radio { initializing, ready, unavailable };
struct Snapshot {
  uint8_t graphicsFailure = 0; // 1: PSRAM missing, 2: allocation failed
  bool running = false;
  uint64_t uptimeSeconds = 0;
  Link link = Link::disconnected;
  char address[16] = {};
  Web web = Web::starting;
  Radio radio = Radio::initializing;
  float frequency = 433.42f;
  char build[27] = {};
};
struct Page { char lines[9][27] = {}; };
void format(const Snapshot &snapshot, Page &page);
enum class Result { pending, ready, failed };
struct Output {
  virtual ~Output() = default;
  virtual Result initialize(uint32_t now) = 0;
  virtual bool row(uint8_t index, const char *text) = 0;
  virtual void brightness(uint8_t value) = 0;
};
class Session {
  Output &output;
  Page painted;
  bool initialized = false, failed = false, attempted = false;
  uint16_t paintedMask = 0;
  uint8_t sleepStage = 0, awakeLevel = 32;
  uint32_t offAfter, dimAfter;
  bool input = false, stableInput = false;
  uint32_t inputSince = 0, awakeSince = 0, lastRow = 0;
public:
  explicit Session(Output &out,uint32_t offAfter=60000) : output(out), offAfter(offAfter), dimAfter(offAfter/2) {}
  void setTimeouts(uint32_t dimMs,uint32_t offMs) {dimAfter=dimMs;offAfter=offMs;}
  void setBrightness(uint8_t value) {awakeLevel=value?value:32;}
  void tick(uint32_t now, const Snapshot &snapshot, bool wakePressed);
};
}

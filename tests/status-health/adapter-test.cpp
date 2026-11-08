#include <assert.h>
#include <string.h>
#include "Arduino.h"
#include "StatusDisplay.h"
#define SOMFY_H
#define ESPNetwork_h
#define webserver_h
#define utils_h
struct RadioSample { bool displayStatus(bool &ready,float &frequency) {ready=true;frequency=433.42f;return true;} };
struct SomfyShadeController { RadioSample transceiver; } somfy;
struct ESPNetwork { bool softAPOpened=false;bool connecting() const {return false;} } net;
constexpr unsigned LISTEN=1;
struct Web { uint8_t listenerState() const {return LISTEN;} } webServer;
struct ConfigSettings { struct {char name[15]="4.0.2";} appVersion; } settings;
struct LoopStats {
  bool ready=false;unsigned reads=0;
  uint32_t rate=400,maximum=12345;
  bool hasPublished() const {return ready;}
  uint32_t publishedRate() {++reads;return rate;}
  uint32_t publishedMaxUs() {++reads;return maximum;}
} loopStats;
uint32_t fakeNow=0;int pins[64]={};bool attachOK=true;SerialStub Serial;
#include "../../src/StatusAdapter.cpp"
int main() {
  auto sample=StatusPanel::snapshot(true);
  assert(sample.health.sampled && sample.health.psramPresent);
  assert(sample.health.internalFreeBytes==65536 && sample.health.internalMinimumBytes==32768 && sample.health.internalLargestBytes==16384);
  assert(sample.health.psramFreeBytes==6u*1024*1024);
  assert(!sample.health.loopSampled && loopStats.reads==0);
  assert(strstr(sample.build,"CC1101 / Plus UI") && strcmp(sample.firmwareVersion,"health-test")==0);
  loopStats.ready=true;sample=StatusPanel::snapshot(true);
  assert(sample.health.loopSampled && sample.health.loopRate==400 && sample.health.loopMaxUs==12345 && loopStats.reads==2);
  loopStats.rate=loopStats.maximum=0;
  FakeHeap::internalFree=FakeHeap::minimum=FakeHeap::largest=FakeHeap::psramFree=0;
  sample=StatusPanel::snapshot(true);
  assert(sample.health.loopSampled && !sample.health.loopRate && !sample.health.loopMaxUs && loopStats.reads==4);
  assert(sample.health.psramPresent && !sample.health.psramFreeBytes && !sample.health.internalFreeBytes);
  FakeHeap::psramTotal=0;const auto reads=FakeHeap::psramReads;
  sample=StatusPanel::snapshot(true);assert(!sample.health.psramPresent && FakeHeap::psramReads==reads);
}

#include "ShadeRequests.h"
#include "CounterCommandGuard.h"
#include <assert.h>
#include <nvs.h>
#include "esp_log.h"
static const char *TAG="owned repeat test";
#define SOMFY_MAX_GROUPED_SHADES 32
struct SomfyShadeOps {uint32_t sendOwnedRepeat(const shade_request_t &);};
#include "production-owned.inc"
int main() {
 FakeNvs::reset();SomfyShadeOps ops;
 auto &shade=somfy.shades[0];shade.proto=radio_proto::RTS;shade.bitLength=80;
 shade.setRemoteAddress(123);shade.flipCommands=true;
 shade_request_t req;req.type=shade_request_type_t::shade_repeat;req.id=1;req.command=somfy_commands::Up;req.repeat=17;
 auto ticket=ops.sendOwnedRepeat(req);assert(ticket==201 && shade.lastFrame.cmd==somfy_commands::Down);
 assert(shade.lastRollingCode==1 && shade.state().target==0);
 auto writes=storageWrites;
 auto repeated=ops.sendOwnedRepeat(req);assert(repeated==202 && storageWrites==writes && shade.lastRollingCode==1);
 // Changed step must dispatch a new command, not repeat the old encoded step.
 req.command=somfy_commands::StepUp;req.stepSize=3;
 assert(ops.sendOwnedRepeat(req)==203 && shade.lastFrame.stepSize==3);
 req.stepSize=4;assert(ops.sendOwnedRepeat(req)==204 && shade.lastFrame.stepSize==4 && shade.lastRollingCode==3);
 req.type=shade_request_type_t::group_repeat;req.command=somfy_commands::Down;req.stepSize=0;
 auto &group=somfy.group;group.proto=radio_proto::RTS;group.bitLength=80;group.setRemoteAddress(456);group.linkedShades[0]=1;
 assert(ops.sendOwnedRepeat(req)==205 && group.direction==1 && group.events==1 && shade.state().target==100);
 writes=storageWrites;assert(ops.sendOwnedRepeat(req)==206 && storageWrites==writes && group.events==1);
 const auto saved=group.lastRollingCode;somfy.transceiver.reject=true;req.command=somfy_commands::Up;
 assert(!ops.sendOwnedRepeat(req) && group.lastRollingCode==saved && group.direction==1 && group.events==1);
 somfy.transceiver.reject=false;
 // Unsupported paths must refuse before counter writes/reads or model effects.
 const auto reads=FakeNvs::reads;writes=storageWrites;const auto frames=somfy.frames;
 for(auto cmd:{somfy_commands::My,somfy_commands::Stop,somfy_commands::Prog,somfy_commands::Sensor}) {
  req.command=cmd;assert(!ops.sendOwnedRepeat(req));
 }
 req.command=somfy_commands::Up;req.type=shade_request_type_t::shade_repeat;
 shade.proto=radio_proto::GP_Remote;assert(!ops.sendOwnedRepeat(req));shade.proto=radio_proto::RTS;
 shade.tiltType=tilt_types::euromode;assert(!ops.sendOwnedRepeat(req));shade.tiltType=tilt_types::none;
 req.command=somfy_commands::Toggle;shade.bitLength=56;assert(!ops.sendOwnedRepeat(req));
 req.command=somfy_commands::Up;req.target=40;assert(!ops.sendOwnedRepeat(req));req.target=-1;
 req.repeat=0;assert(!ops.sendOwnedRepeat(req));
 assert(FakeNvs::reads==reads && storageWrites==writes && somfy.frames==frames);
 // A configuration change must never reuse a frame with the previous wire
 // identity, even when command, step and rolling code still match.
 for(unsigned changed=0;changed<4;++changed) {
  req={};req.type=shade_request_type_t::shade_repeat;req.id=1;
  req.command=somfy_commands::Up;req.repeat=11;
  shade.proto=radio_proto::RTS;shade.bitLength=80;shade.lastFrame.valid=false;
  assert(ops.sendOwnedRepeat(req));
  const auto code=shade.lastRollingCode;const auto committed=storageWrites;
  if(changed==0)shade.setRemoteAddress(shade.getRemoteAddress()+1);
  if(changed==1)shade.proto=radio_proto::RTW;
  if(changed==2)shade.bitLength=56;
  if(changed==3)shade.bitLength=0; // current effective default is 56
  const auto expectedBits=shade.bitLength?shade.bitLength:somfy.transceiver.config.type;
  assert(ops.sendOwnedRepeat(req));
  assert(storageWrites==committed+1);
  assert(shade.lastRollingCode==(changed==0?1:uint16_t(code+1)));
  assert(shade.lastFrame.remoteAddress==shade.getRemoteAddress());
  assert(shade.lastFrame.proto==shade.proto && shade.lastFrame.bitLength==expectedBits);
 }
 // Separate LCD intents use command variants and consume fresh counters even
 // when the last wire command matches. A second UP can stop the actual motor.
 req.type=shade_request_type_t::shade_command;
 auto before=shade.lastRollingCode;assert(ops.sendOwnedRepeat(req));
 assert(shade.lastRollingCode==uint16_t(before+1));
 before=shade.lastRollingCode;assert(ops.sendOwnedRepeat(req));
 assert(shade.lastRollingCode==uint16_t(before+1));
 req.type=shade_request_type_t::group_command;req.command=somfy_commands::Down;
 before=group.lastRollingCode;assert(ops.sendOwnedRepeat(req));
 assert(group.lastRollingCode==uint16_t(before+1));
 before=group.lastRollingCode;assert(ops.sendOwnedRepeat(req));
 assert(group.lastRollingCode==uint16_t(before+1));
 req.type=shade_request_type_t::shade_repeat;req.command=somfy_commands::Up;
 shade.lastFrame.valid=false;assert(ops.sendOwnedRepeat(req));
 // Zero configured bits can still repeat when its effective default matches.
 shade.bitLength=0;const auto code=shade.lastRollingCode;
 assert(ops.sendOwnedRepeat(req) && shade.lastRollingCode==code);
}

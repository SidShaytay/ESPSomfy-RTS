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
 auto writes=FakeNvs::sets;
 auto repeated=ops.sendOwnedRepeat(req);assert(repeated==202 && FakeNvs::sets==writes && shade.lastRollingCode==1);
 // Changed step must dispatch a new command, not repeat the old encoded step.
 req.command=somfy_commands::StepUp;req.stepSize=3;
 assert(ops.sendOwnedRepeat(req)==203 && shade.lastFrame.stepSize==3);
 req.stepSize=4;assert(ops.sendOwnedRepeat(req)==204 && shade.lastFrame.stepSize==4 && shade.lastRollingCode==3);
 req.type=shade_request_type_t::group_repeat;req.command=somfy_commands::Down;req.stepSize=0;
 auto &group=somfy.group;group.proto=radio_proto::RTS;group.bitLength=80;group.setRemoteAddress(456);group.linkedShades[0]=1;
 assert(ops.sendOwnedRepeat(req)==205 && group.direction==1 && group.events==1 && shade.state().target==100);
 writes=FakeNvs::sets;assert(ops.sendOwnedRepeat(req)==206 && FakeNvs::sets==writes && group.events==1);
 const auto saved=group.lastRollingCode;somfy.transceiver.reject=true;req.command=somfy_commands::Up;
 assert(!ops.sendOwnedRepeat(req) && group.lastRollingCode==saved && group.direction==1 && group.events==1);
 somfy.transceiver.reject=false;
 // Unsupported paths must refuse before counter writes/reads or model effects.
 const auto reads=FakeNvs::reads;writes=FakeNvs::sets;const auto frames=somfy.frames;
 for(auto cmd:{somfy_commands::My,somfy_commands::Stop,somfy_commands::Prog,somfy_commands::Sensor}) {
  req.command=cmd;assert(!ops.sendOwnedRepeat(req));
 }
 req.command=somfy_commands::Up;req.type=shade_request_type_t::shade_repeat;
 shade.proto=radio_proto::GP_Remote;assert(!ops.sendOwnedRepeat(req));shade.proto=radio_proto::RTS;
 shade.tiltType=tilt_types::euromode;assert(!ops.sendOwnedRepeat(req));shade.tiltType=tilt_types::none;
 req.command=somfy_commands::Toggle;shade.bitLength=56;assert(!ops.sendOwnedRepeat(req));
 req.command=somfy_commands::Up;req.target=40;assert(!ops.sendOwnedRepeat(req));req.target=-1;
 req.repeat=0;assert(!ops.sendOwnedRepeat(req));
 assert(FakeNvs::reads==reads && FakeNvs::sets==writes && somfy.frames==frames);
}

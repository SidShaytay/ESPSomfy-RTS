# Advanced remote commands on the LCD

**Navigation version:** These paths apply to **4.8.0 and later**. For 4.7.3, see the [versioned path map](lilygo-local-ui.md#navigation-versions).

Open **This controller → Configure → Rooms, devices & groups → Devices → Advanced**, choose the device, then
**Advanced remote**. For a group, open **Groups → Advanced**, choose the group,
then **Advanced remote**. Its separate **Member links** page keeps configuration
repairs distinct from commands sent to motors.

Choose **Up + Down**, **MY + Up + Down**, **MY + Up**, **MY + Down**, or **Prog**.
The **Repeats** setting selects 1–127 repeats for this explicit request; it is
not a duration or an automatic hold-until-jog sequence. Review the target,
command and repeat count. **Cancel** is selected first. These commands can move
a motor or change its limits or pairing.

Confirmation queues one request through the same typed command service as the
web remote. A pending request can be cancelled with Back before execution.
Changing or losing the target, authorization, session or configuration, or
waiting five seconds in the queue, prevents that queued request from executing.
An admitted request can wait up to five seconds for its FIFO turn and radio
space. Execution rechecks the target and admission. If a busy radio or active
maintenance refuses execution, there is no automatic retry. Cancel cannot
retract a command already accepted by the radio.

**Request accepted** means the controller accepted the request. It does not
confirm RF reception, a physical jog, pairing, limit changes or a saved model
file. Observe the motor before deciding what to do next. Prog does not change
the controller’s paired marker automatically. **Paired marker** remains a
separate configuration-only action after physical pairing has been observed.

The commands are available for configured RTS, RTW and RTV virtual remotes with
valid 56/80-bit settings. GPIO devices do not acquire invented RF chord behavior.
Groups use their own remote identity; this is not a member-by-member programming
wizard. Favourite programming, sensor simulation and automated pairing/limit
sequences are separate requirements. Offline tests and previews use fake
hardware and do not establish physical behavior.

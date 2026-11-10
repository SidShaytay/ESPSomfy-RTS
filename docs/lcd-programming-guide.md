# Pair or unpair from the LCD

The LCD guides you through sending a programming request to one RF shutter and recording the result you have verified. The guide is implemented in main and covered by offline input, renderer, adapter and FIFO tests. Physical motor programming is not established by that evidence.

## Use the guide

1. Unlock administration and choose the device under Advanced, then **Pair/unpair guide**.
2. Choose **Pair controller** or **Unpair controller**. The displayed paired marker is a saved setting, not a reading from the motor.
3. Follow your motor’s instructions to enter its learn mode. Choose **I observed learn mode** only after checking it yourself. Opening the guide sends nothing.
4. Leave the repeat count at 1 unless the motor’s instructions require another value. The supported range is 1–127. Choose **Review and send**.
5. Check the device name, intended action and repeat count. The review initially selects **Cancel**. Choose **Send programming request** deliberately; this can change motor membership. On a one-button motor, Prog may operate the motor.
6. **Request accepted** means the controller accepted the request. A jog can mean either adding or removing a controller; software cannot confirm which happened. Verify the intended result using the motor’s instructions before choosing **I verified intended result**.
7. Review the separate paired-marker change. Choose **Save marker: Paired** or **Save marker: Unpaired** only when appropriate. This saves configuration and sends no further programming request.

Cancel or Back leaves unsent requests unsent. Cancellation cannot recall frames already accepted for transmission. There is no automatic retransmission after a refusal; inspect the motor state before deliberately starting again. A failed marker-file save offers only a file-save retry, never another programming command.

## When the request cannot run

The guide requires a supported RF device, current authorization and unchanged device configuration. It refuses dispatch while the controller is busy, the radio is unavailable or paused, the identity is quarantined, or the relevant movement/work has not finished. A queued request expires after five seconds. GPIO devices remain configured but do not offer this RF programming guide.

The guide does not read motor membership, reset rolling codes, set motor limits or automate entering learn mode. The existing paired-marker editor remains available for correcting metadata without sending RF.

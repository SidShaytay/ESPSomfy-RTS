# Historical 4.7.3 network-event stack exhaustion

[Issue #34](https://github.com/SidShaytay/ESPSomfy-RTS/issues/34) concerns a preserved engineering 4.7.3 crash, before the 4.8.0/4.9.0/4.9.1 installations. Later full readbacks retained the same diagnostic bytes. This is not evidence of a new crash in those releases.

## Evidence and cause

The private pre-4.9.0 readback contains a 30,820-byte ELF-format coredump with valid CRC32 `107206f1`. Its source hash prefix and the historical serial log match the recovered release ELF, whose full SHA256 is `165e6d5d4448c9ab7fbecc708af6c7103bcdf38c7e8b17038ae234e503979f5e`, source/tag `1fb7abc1552b52e21fd1e1892930e77d43ca2ff9` / `v4.7.3`. The embedded hash alone is a truncated prefix; the full digest comes from the recovered ELF. Recovery verified the content-addressed artifact from the worktree cleanup archive without rebuilding the old release.

The serial log reports `Stack canary watchpoint triggered (arduino_events)` and a corrupted short backtrace. Offline decoding with esp-coredump 1.18.1 and the retained Xtensa ESP32-S3 GDB recovers this caller chain:

```text
NetworkEvents::_checkForEvent
  ESPNetwork::networkEvent(WIFI_STA_GOT_IP)
    ESPNetwork::setConnected
      SSDPClass::begin
        SSDPClass::_sendNotify
          AsyncUDP::writeTo
            tcpip_api_call / UDP / IPv4 / Ethernet output
              Wi-Fi transmit / buffer allocation
                heap critical-section exit / Xtensa exception
```

The callback ran on `arduino_events`, with a 4,096-byte allocation. Its saved stack pointer was only 12 bytes above the stack base. `_sendNotify(UPNPDeviceType*, bool)` holds a 1,460-byte local packet buffer; its compiled frame reserves 1,616 bytes throughout the synchronous transmit chain. Together, the watchpoint, exhausted stack and recovered callers identify stack exhaustion during SSDP startup transmission. The top unwound frame is damaged, as expected at the stack boundary; the diagnosis does not depend on interpreting that frame as an application call.

The crash occurred after getting an IP during the older 4.7.3 boot. It does not require a Home Assistant request. Sid's observed v4.0.x Home Assistant offline episodes could have many causes; there is no corresponding crash evidence to establish a connection. Older builds are used only as evidence; no backport is planned.

## Current-source correction

Before the fix, `ESPNetwork.cpp` and `SSDP.cpp` on main were identical to 4.7.3, so the failing path remained present even though the later verified boots did not panic.

`setConnected()` now publishes an atomic pending flag. `ESPNetwork::loop()` consumes it after connection admission and initializes/reinitializes SSDP metadata, mDNS services and SSDP from the loop task. This removes discovery transmission from both Wi-Fi and Ethernet event callbacks. The pinned SDK gives the loop task an 8,192-byte stack. No event-stack increase, new task, allocation or SSDP packet-format change is introduced.

A pending notification survives a disconnect until connected again; repeated notifications coalesce. Existing connection metadata/counters are still updated in the callback. Existing loop handling retries SSDP startup and applies the enabled/disabled setting. The change does not redesign all shared network state or establish a general guarantee against unrelated crashes.

## Verification and limits

`CXX=<host-compiler> bash tools/test-network-discovery.sh` compiles the production callback, connection setup, loop, discovery helper and connection-state checks with inert network dependencies in Wi-Fi-only and Ethernet variants. Discovery operations assert that they execute outside the event callback. The test covers initial connection, reconnect, a disconnect before processing, SSDP disabled/re-enabled, repeated notifications, link loss before its callback and unreadable-settings admission.

The unmodified source fails the task-context assertion. The corrected source passes. Inverting the connected guard also fails the regression, confirming that it checks the deferred behavior. These are task-boundary tests, not an emulation of the ESP32 stack overflow.

Build and existing-test results are recorded in `tasks/issue-34-panic.md`. Raw dumps, decoded memory, logs and identifiers stay outside Git. The investigation makes no controller, USB or Home Assistant accesses and performs no resets, flashing, RF/motor commands or fault injection. A firmware build does not establish installed-device or combined-load health; follow [issue #33](https://github.com/SidShaytay/ESPSomfy-RTS/issues/33) for the remaining physical/load qualification.

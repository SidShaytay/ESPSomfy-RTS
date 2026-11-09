# Recover a model that could not be loaded

If the controller cannot safely read its model at startup, it preserves the existing model file and refuses managed restart, firmware-update admission, model edits, commands and GPIO output initialization. An ordinary Save current model cannot replace the file with empty or partially decoded state. Retained MQTT entity/discovery records are preserved: an unavailable model cannot announce online, publish model data, delete discovery or accept MQTT commands. If trust is lost on a connected client, it attempts one retained offline status and disconnects. Trusted recovery reconnects through the normal full publication path.

Network diagnostics and authenticated backup upload remain available; an unavailable model is not a healthy boot for firmware confirmation.

Use a complete, validated model backup with **Shades and Repeaters** selected to recover. Shades includes rooms, devices and groups; Repeaters also explicitly replaces the repeater list, including an empty list. Settings-only, repeaters-only and shades-only restore cannot clear a failed model load. Successful recovery requires the checked current-model file save to finish; a failed save leaves model commands blocked. This does not authorize restoring stale radio counters: the existing higher file/NVS counter rule remains in effect, and no RF command is replayed.

A genuinely new controller with a healthy mounted filesystem and a proven missing model file may add and save its first model normally. Until the first checked successful current-model commit, it withholds model MQTT publication and online availability, preserving any prior retained discovery. A failed save does not qualify it. Mount, read, allocation, validation and close errors do not count as an empty controller. Startup explicitly disables format-on-mount-failure; it does not erase configuration to recover.

## Implementation and verification

Loading captures at most 64 KiB of immutable input before validation or adoption. Canonical CC1101/Plus firmware uses PSRAM without an internal-heap fallback; generic boards use a bounded normal allocation. The snapshot is wiped on every release. Complete reads, EOF and close are checked. A successful startup mount plus a current LittleFS mount/info check is required; missing means both stat and open report exactly `ENOENT`.

The existing version-aware record readers keep optional-field defaults and historical network/transceiver size discrepancies. Declared model/repeater records and backup tails must finish, and undeclared trailing bytes are refused. All validation uses the same immutable bytes later adopted, avoiding a second filesystem read after validation. Business-value repair rules remain separate from physical read success.

The trust flag is bounded and loop-owned. It does not make restore transactional, roll back NVS or certify the existing settings-restore outcomes. If adoption or checked persistence fails, the model remains quarantined; already advanced rolling-code maxima are not reduced. This change does not activate the standalone updater or authorize hardware testing.

Offline checks: `bash tools/test-model-load.sh` exercises the actual ConfigFile load/restore and controller startup bodies with fake effect sinks plus injected snapshot I/O failures. The actual SafeRestart service, GPIO/transport, explicit-counter, model-commit and model-edit harnesses check refusal before their effects. Shared and generic SDK builds check target linkage.

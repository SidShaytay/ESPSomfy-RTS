# Security settings load trust

Security configuration starts **unknown**. Boot reads the existing `SEC`
namespace read-only into one bounded temporary record, then publishes it only
when every present field was read completely and the active credentials are
usable. Both staging and replaced credentials are explicitly wiped.

An exact namespace `NOT_FOUND`, or an existing namespace with the type and all
three credential keys exactly `NOT_FOUND`, represents a legitimate empty first
boot. The latter preserves old boots that created an empty namespace. Other
open/read/type/length failures are unavailable, not `None`. A missing type with
any credential key present is refused. A stored explicit `None` remains valid;
PIN requires exactly four bytes and password mode requires nonempty username
and password, each at most 32 bytes. Bytes are preserved without trimming.

Unknown or failed loads refuse web tokens, login, socket authentication and
upload admission. The LCD exposes unavailable authorization. A failed reload
revokes the local session and STOP lease. Saving or reconciling default settings
cannot clear failed read trust or create replacement credentials. This introduces
no reset, recovery password or credential fallback.

This checkpoint supplies `SecuritySettings::loadTrusted()` and makes
`ConfigSettings::begin()` report security load failure. Restart/update/postboot
health consumers still need to consume that predicate before this feature is
complete. It is independent of `SettingsDurability`, which tracks writes during
the current boot. Other settings namespaces still use unchecked legacy loading;
this checkpoint does not certify them or their automatic migration as healthy.

`test_securityload` injects NVS open and every read failure, type mismatch,
overlength and embedded terminators, missing credentials and invalid modes. It
also exercises the actual settings load/save boundary. `tools/test-security-load.sh`
compiles production web authentication methods and the real LCD adapter with
fake transport/NVS, proving that failed or blank credential loads cannot log in
and a known empty first boot still works. No controller is contacted.

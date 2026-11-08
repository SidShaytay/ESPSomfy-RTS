# Updater certificate time adapter

The reviewed shared build pins ESP-IDF **5.5.5** and Mbed TLS **3.6.6**. Offline inspection found `CONFIG_MBEDTLS_HAVE_TIME=1` but no `CONFIG_MBEDTLS_HAVE_TIME_DATE` in the actual `esp32s3/qio_opi/include/sdkconfig.h`. The supplied `mbedtls/esp_config.h` therefore undefines `MBEDTLS_HAVE_TIME_DATE`. In the actual prebuilt library, `mbedtls_x509_time_is_past()` and `mbedtls_x509_time_is_future()` both return constant zero, and the CRT verifier has no calls to those helpers. A source clock floor alone cannot provide certificate validity-window checks. Application `-D` flags or linker wrappers around those unused helpers do not repair the already-built verifier.

`GitUpdateCertificate` adds an updater-client-specific adapter while retaining the pinned SDK and its CA bundle. The trust policy remains verified GitHub HTTPS with certificate chain, hostname and validity-window checks; no additional publisher key or trust store is introduced. This module does not start networking or install a global verification callback.

## Per-client ownership

A certificate session is constructed before the updater's HTTP client and remains alive until that client's cleanup finishes. Its task-local `Scope` supplies the context to the SDK's userdata-free `crt_bundle_attach` hook. The scope saves/restores its previous pointer for nested use. Each session attaches once; unexpected reuse, a null original callback or an unsupported SDK/configuration fails closed.

After invoking the original `esp_crt_bundle_attach`, the adapter reads its exact callback and opaque context using the pinned header's `MBEDTLS_PRIVATE` member names. Static type checks, exact SDK/Mbed TLS/config-file guards and a target ABI fingerprint make this dependency explicit. The reviewed S3 `mbedtls_ssl_config` has size 196, alignment 4, verify callback offset 64 and opaque-context offset 68. Numeric static assertions validate these properties; all accesses still use typed member names. Host fixtures have a separate guarded shape, and firmware compilation rejects host-test hooks. There are no hardcoded runtime offsets. Canonical package validation rejects additional `MBEDTLS_*` / `CONFIG_MBEDTLS_*` compiler definitions or raw flag overrides; the sole permitted selection is the required `MBEDTLS_CONFIG_FILE="mbedtls/esp_config.h"`. The public `mbedtls_ssl_conf_verify` setter modifies only this SSL configuration. Other clients retain their own bundle attachment and callback.

Verification receives the per-config session pointer and never consults the task-local factory slot. It calls the original callback once with the same certificate, depth, flag pointer and opaque value, including null opaque values. The original callback may legitimately clear NOT_TRUSTED after bundle signature verification. Its resulting flags and exact return value are preserved; the adapter only adds time failure flags afterward.

Dates must be valid Gregorian UTC calendar values with ordered validity bounds. Both endpoints are inclusive. Every peer-chain certificate is checked, including intermediates and a peer-supplied root if present. The first callback anchors one UTC timestamp for the entire chain; every callback freshly checks the compiled source floor, refuses clock rollback and refuses a forward jump exceeding the operation's 180-second window. A missing clock or invalid/zero date cannot become an exception to verification. All failures persist in the session, even if a later certificate is individually valid.

The worker must explicitly use synchronous HTTP, keep its session/scope alive through client cleanup, and refuse a successful-open result unless the original verification succeeded and the leaf callback actually ran. Cancellation only changes a flag; another thread never destroys the client's callback context.

## Evidence from the actual SDK

No matching host Mbed TLS library or source tree was available offline. The following evidence comes from the actual pinned archives and their matching supplied headers, not a replacement host implementation:

| Archive | SHA256 |
| --- | --- |
| `esp32s3/lib/libmbedx509.a` | `571b9da50879f8699aeabf55bf35e8de0d1a83af06539bb66a7d5815005b4eb2` |
| `esp32s3/lib/libmbedtls.a` | `e1d9403acd4929820b147ea53147d2f366a4b5f0aaf0ad3263a9df56f85718e8` |

The actual `mbedtls_ssl_conf_verify` setter stores its two arguments at offsets 64/68; the actual `mbedtls_ssl_config_init` clears 196 bytes. This independently matches the compiler/header ABI fingerprint. A real target compile with unchanged SDK/Mbed versions and `-DCONFIG_MBEDTLS_SSL_DTLS_CONNECTION_ID=1` previously passed the type guards and changed config size to 200; it is now refused by the fingerprint. A host padding mutation also changes layout while preserving callback types and is refused. Twelve effective/resolved crypto-override package cases first failed their rejection tests, then passed after the compiler-evidence guard.

`esp_crt_bundle_attach` in `esp_crt_bundle.c.obj` calls `mbedtls_ssl_conf_ca_chain` with `s_dummy_crt`, then `mbedtls_ssl_conf_verify` with `esp_crt_verify_callback` and its opaque pointer. Dumping `.rodata.s_dummy_crt` gives 408 entirely zero bytes.

In `x509_crt.c.obj`, `x509_crt_verify_restartable_ca_cb$constprop$0$isra$0` constructs the chain before invoking its verification callbacks. Its `+0xe6/+0xe9` instructions save the child's issuer pointer; `+0x20d` forms the candidate parent's subject pointer. Cross-compiling `offsetof` probes with the actual compiler metadata establishes issuer offset 80, subject offset 112 and next pointer 0x194. These values are evidence for interpreting the binary, not offsets used by the adapter.

The parent loop at `+0x216/+0x219` calls `x509_name_cmp`. A name mismatch follows `+0x21e → +0x220` to the parent's next pointer. `x509_name_cmp` compares the first ASN.1 OID tags at `+0x8..+0xf`; a well-formed nonempty peer issuer has OID tag 6, while the dummy's subject is all zero. The dummy therefore cannot be selected as a parent of such a peer chain. At the chain's end `+0x32d` adds NOT_TRUSTED to the last actual peer certificate; the bundle verifier can then validate that peer's issuer key. The dummy is not blindly exempted from date checking: any zero-date certificate reaching our callback is refused.

The callback walk at `+0x46e..+0x47a` passes the stored context, certificate, depth and local flags to the configured callback. On a zero callback return, `+0x459..+0x468` ORs the resulting flags into the aggregate. At `+0x352..+0x35b` the verifier returns zero only when the aggregate is zero; otherwise its literal is `0xffffd900`, `MBEDTLS_ERR_X509_CERT_VERIFY_FAILED` (`-0x2700`). Thus added time flags cause actual SDK verification failure. The matching public headers document the same per-certificate callback and flag behavior.

Reproduce with the pinned toolchain's `nm`, `objdump -dr` and `objcopy --dump-section`, using the archives above. No SDK archive is modified. A forced-link build additionally resolves the real worker, certificate adapter and global allocation-hook APIs without invoking them.

## Offline tests and limits

`tools/test-update-certificate.sh` checks valid/expired/future/intermediate certificates, inclusive bounds, leap/century/calendar errors, missing/floor/rollback/forward-jump clocks, original error/flag preservation, null opaque context, callback absence/reuse, nested scopes, verification after factory-scope exit and concurrent task-local attachment. Negative compilations reject unreviewed SDK, Mbed TLS, callback types and configuration selection. Worker tests use the real adapter with scripted SDK boundaries.

These tests and binary call-flow evidence establish the adapter's offline contract. They do not execute an actual SDK TLS handshake or establish a GitHub connection, current controller clock, live memory headroom, certificate compatibility or latency. Those qualification stages still need explicit authorization. Startup, routes, global allocator initialization and installation remain inactive until their separate integration reviews.

# Security assessment #14 — pinned cjkas + LILYGO Plus candidate

Source under assessment: `d7721a93a57da08b026aa30914922e0e0995395f` on `feature/13-cjkas-lilygo-plus`, initially held in the local `issue-13-cjkas` worktree. Baseline: cjkas v4.0.2 commit `07bffdec6b540cc99f31d02d7e6de6d3d1e7f8f3`. The assessment worktree is `security/14-assessment`; the deployed v2.4.7 controller was not assessed or contacted.

## Scope and threat model

All 267 tracked source files were in repository scope, including inherited firmware, dependencies, vendored code, web assets, LILYGO port, tooling and CI. The main threats considered were an unauthenticated LAN client, a nearby radio actor who can advertise a WiFi network name, a client with valid device authentication, a malicious or malformed backup, and untrusted build dependencies/artifacts. Security mode None deliberately permits local control; Secured mode requires an API token for protected HTTP and WebSocket operations. No network traffic, RF transmission, hardware tests, flashing, backups or credentials were used. Local host tests used synthetic data.

## Tools and evidence

- Codex Security CLI 0.3.0, plugin 0.1.95, `gpt-6-sol` at high reasoning with `daybreak_blue` requested. Full repository Standard scan ID `bd61dca9-a194-4ecb-bce9-5eb67fde011a`, source SHA above, completed 2026-11-05. It investigated 51 security-relevant files in a 267-file scope and yielded two findings after its own static validation. No exhaustive line-by-line review is claimed. Reports and artifacts are private under `~/.local/state/espsomfy-security-14/daybreak-scan/`; the raw report is not published.
- Focused independent source reviews of authentication, WebSocket, upload/restore, web rendering, parser, radio/board, and build paths. A synthetic Node harness established the SSID-to-HTML-sink flow; it did not execute a browser payload.
- Gitleaks 8.30.1 scanned the repository tree; its six alerts matched intentionally synthetic API-token fixtures in `test/test_apitoken/test_main.cpp`. No real credential was established by these alerts. Git history was not fully secret-scanned.
- Zizmor 1.22.0 examined all five workflow files without alerts. Semgrep, cppcheck, and pip-audit reports are retained privately for independent triage; an alert alone is not a confirmed vulnerability.
- Baseline checks: native PlatformIO tests 632 passed, 1 pre-existing skip; Node web suite 60 passed; version script, manifest Python tests (23), and board profile host checks passed. These establish baseline behavior, not security of the device.

## Findings and disposition

- [#16](https://github.com/SidShaytay/ESPSomfy-RTS/issues/16) — High: radio-origin WiFi SSID reaches active HTML in the authenticated UI. Source path confirmed; real-browser execution has not been tested. Inherited from the pinned baseline.
- [#17](https://github.com/SidShaytay/ESPSomfy-RTS/issues/17) — Medium: SSDP request-line parser writes a terminator one byte beyond its stack buffer. Source bound confirmed; device impact beyond memory corruption is unproven. Inherited from the pinned baseline.
- [#18](https://github.com/SidShaytay/ESPSomfy-RTS/issues/18) — High: crafted backup record counts pass validation and exceed fixed model arrays during restore. Source path and synthetic host harness confirmed; hardware effect untested. Inherited from the pinned baseline.
- [#19](https://github.com/SidShaytay/ESPSomfy-RTS/issues/19) — Medium: privileged bit-length edit can drive reads beyond a ten-byte transmit payload. Source path and synthetic host harness confirmed; radio effect untested. Inherited from the pinned baseline.

The assessment remains open while dependency/static alerts are triaged, focused fixes receive regression tests and review, and final source/fix SHAs are recorded. The scanner's completed state does not establish that the project is secure. CodeQL's C++ workflow builds only the native subset and does not trace all firmware paths; no actual device configuration or broker ACL was inspected.

Dependency and static alert triage: pip-audit flagged Starlette in the host pioarduino tool environment and `ecdsa` in the separate USB backup environment; neither is linked to a demonstrated firmware or documented CLI build attack path. Semgrep's cleartext `ws://` token transport is real and is recorded as a residual deployment risk under the documented local-network threat model. Its Django CSRF matches do not apply to static HTML; Gitleaks matches are fixed test fixtures. Zizmor reported no workflow findings. Cppcheck corroborated #17; its other warnings were not established as security defects. Version-only Python pins and the PlatformIO release ZIP lack committed content hashes, limiting supply-chain reproducibility. These dispositions and advisory details are retained in the private supply review.

The HTTP upload handlers share global success state and a restore temp path across requests. A second authenticated upload can interfere with the first; the handler also revalidates the file before applying it. No lower-privilege bypass was established, so this remains a robustness/integrity follow-up in the private review rather than a confirmed security finding. Plain HTTP/WebSocket token transport and the absence of token expiry are documented design risks outside the upstream LAN-only threat model; deployments with untrusted LAN observers need a separate transport decision.

Focused local fixes: #16 is committed as `a2f760bd703bafa265a5a69bc15b2095d1e36086` on `security/16-ssid-fix`, with a baseline-failing regression and 62 passing JavaScript tests. #17 is committed as `ed41c159ebbf3f0e7a01563b75381eae4de5d446` on `security/17-ssdp-fix`; the host boundary regression failed before the fix and passed afterward, 633 native tests passed (1 pre-existing skip), and the pinned ESP32-S3 debug build succeeded. Neither fix has been merged, pushed, deployed or flashed. The original reviewed SHA remains the assessment target.

## Reproduction constraints

A local reviewer can check the original snapshot with `git rev-parse HEAD`, run `node --test 'test/js/*.test.js'`, `pio test -e native`, `bash test/scripts/test_set_version.sh`, `python3 -m unittest discover -s tools -p 'test_*.py'`, and `CXX=g++ bash tools/test-board-profile.sh` with the pinned toolchain described in `LILYGO.md`. The scan can be rerun against the exact source SHA with Codex Security CLI 0.3.0 using repository scope, GPT-6 Sol/high and an authorized Daybreak Blue account; output belongs outside the repository. Scanner configuration and the private manifest must be checked before comparing results.

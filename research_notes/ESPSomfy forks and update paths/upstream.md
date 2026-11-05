# Upstream ESPSomfy RTS maintenance status

Research cutoff: 2026-11-03, America/Los_Angeles. Read-only GitHub REST/GraphQL queries; no controller, LAN, motor or repository mutations.

## Default branch and releases

### Takeaway
Upstream code/release development is dormant: the last default-branch commit and prerelease date to August 2024. Repository metadata updates and community discussion in 2026 must not be mistaken for code maintenance. [Commit](https://github.com/rstrouse/ESPSomfy-RTS/commit/eb75868adb92b3e6f5d3ead069f915d84a72efde), [releases](https://github.com/rstrouse/ESPSomfy-RTS/releases)

### Cited Findings
- Current default branch is `main`; repository is not archived or disabled. GitHub API reports `pushed_at=2024-08-19T17:46:40Z`; `updated_at=2026-11-02T11:43:33Z` therefore does not demonstrate new code. API also reports 177 forks and 215 combined open issues/PRs at query time. [Repository metadata](https://api.github.com/repos/rstrouse/ESPSomfy-RTS)
- Last default-branch commit is `eb75868adb92b3e6f5d3ead069f915d84a72efde`, 2024-08-19 17:46 UTC, “Update build params for C3 #458 #454”. Previous substantive fixes were Ethernet/Wi-Fi fallback on August 4 and hidden-SSID behavior on July 27. [Latest commit](https://github.com/rstrouse/ESPSomfy-RTS/commit/eb75868adb92b3e6f5d3ead069f915d84a72efde), [history](https://github.com/rstrouse/ESPSomfy-RTS/commits/main/)
- Latest prerelease is v2.4.7, published 2024-08-19 17:46:41 UTC; GitHub latest stable is v2.4.6, published 2024-07-26 00:18:27 UTC. [v2.4.7](https://github.com/rstrouse/ESPSomfy-RTS/releases/tag/v2.4.7), [v2.4.6](https://github.com/rstrouse/ESPSomfy-RTS/releases/tag/v2.4.6)
- Latest merged PR returned by `is:pr is:merged` search sorted by update is #317, merged by rstrouse on 2024-04-03. Direct maintainer commits continued afterward, so PR merge inactivity and commit inactivity are distinct signals. [PR #317](https://github.com/rstrouse/ESPSomfy-RTS/pull/317)

### Inferences
- “Dormant upstream” or “appears unmaintained” is supported; the archive flag does not certify active maintenance, and a recent `updated_at` date is not sufficient evidence of it. [Metadata](https://api.github.com/repos/rstrouse/ESPSomfy-RTS)

### Gaps
- No claim is made about private development or plans not published on GitHub.

## Maintainer response and abandonment claims

### Takeaway
No formal abandonment or maintainer handover declaration was found. The maintainer's latest public issue comment returned by the user GraphQL `issueComments(last:30)` connection is January 20, 2025; later discussion consists of community contributions and questions. [Latest observed maintainer comment](https://github.com/rstrouse/ESPSomfy-RTS/issues/519#issuecomment-2603151859)

### Cited Findings
- The last five public issue comments returned for user rstrouse run from January 17 to January 20, 2025. The final one explains Somfy pairing behavior; it does not announce discontinuation or appoint another maintainer. [Final comment](https://github.com/rstrouse/ESPSomfy-RTS/issues/519#issuecomment-2603151859)
- Community issue #576 asks about the maintainer's absence; comments span June–September 2025 and contain no maintainer response. [#576](https://github.com/rstrouse/ESPSomfy-RTS/issues/576)
- Community issue #607 explicitly asks whether the project was abandoned. January 2026 discussion proposes adding maintainers, and March 2026 commenters express concern; no maintainer statement or confirmed personal explanation appears. Do not repeat speculation about the person's circumstances as fact. [#607](https://github.com/rstrouse/ESPSomfy-RTS/issues/607)
- September 2026 comment labels the project “seemingly” abandoned; it is community opinion, not an official status announcement. [#688 comment](https://github.com/rstrouse/ESPSomfy-RTS/issues/688#issuecomment-5793470589)
- Open community contributions include radio-init hang recovery (#679, July 2026), non-linear position estimation (#671, July 2026), transmission scheduling (#690, September 2026), and allocation corrections (#691, September 2026). Their existence demonstrates community development, not upstream merge activity. [#679](https://github.com/rstrouse/ESPSomfy-RTS/pull/679), [#671](https://github.com/rstrouse/ESPSomfy-RTS/pull/671), [#690](https://github.com/rstrouse/ESPSomfy-RTS/pull/690), [#691](https://github.com/rstrouse/ESPSomfy-RTS/pull/691)

### Inferences
- Treat the lack of upstream maintenance as a practical planning constraint, but avoid declaring the author has formally abandoned it. [History](https://github.com/rstrouse/ESPSomfy-RTS/commits/main/), [community question](https://github.com/rstrouse/ESPSomfy-RTS/issues/607)

### Gaps
- No reliable source establishes why the maintainer stopped responding or whether they will resume.

## Relevant fork and update clues

### Takeaway
Community work already addresses some of the user's questions, but version/release correctness is not automatically solved in an active fork. Two primary threads are worth checking in the fork comparison. [Hardware preset thread](https://github.com/rstrouse/ESPSomfy-RTS/issues/693), [fork bugfix PR](https://github.com/rstrouse/ESPSomfy-RTS/pull/647)

### Cited Findings
- In hardware-presets issue #693, xkain says presets are already implemented in the xkain fork. This is a direct derivative-maintainer claim; the feature/code and exact LILYGO Plus compatibility still require inspection. [#693](https://github.com/rstrouse/ESPSomfy-RTS/issues/693)
- cjkas's PR #647 remains open upstream. In March 2026, a user reported installing fork release 3.0.3 while the device displayed v2.4.8. cjkas explicitly acknowledged the version number was hard-coded and had been forgotten. Therefore a higher GitHub release number alone does not prove that firmware/application version stamping is correct. [User report](https://github.com/rstrouse/ESPSomfy-RTS/pull/647#issuecomment-4141048481), [maintainer acknowledgment](https://github.com/rstrouse/ESPSomfy-RTS/pull/647#issuecomment-4141785449)
- cjkas also confirmed adding S3 binaries and enabling fork issues in March 2026; those are genuine derivative-maintainer responses. Generic S3 binaries are not evidence of LILYGO Plus power/RF-switch support. [S3 binaries](https://github.com/rstrouse/ESPSomfy-RTS/pull/647#issuecomment-4141766592), [issues enabled](https://github.com/rstrouse/ESPSomfy-RTS/pull/647#issuecomment-4142409718)

### Inferences
- Reuse should be evaluated feature-by-feature: presets, OTA repository/asset selection, version stamping and exact board support are separate requirements. [#693](https://github.com/rstrouse/ESPSomfy-RTS/issues/693), [#647](https://github.com/rstrouse/ESPSomfy-RTS/pull/647)

### Gaps
- This workstream does not rank fork activity or claim these forks have solved board-specific OTA; other researchers cover those questions.

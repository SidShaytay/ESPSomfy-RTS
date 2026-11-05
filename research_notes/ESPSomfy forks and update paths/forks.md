# Active ESPSomfy forks and derivatives

## Which maintained line is most active?

### Takeaway
Among the examined firmware projects, **cjkas/ESPSomfy-RTS is the strongest candidate by recent development volume and stable-release activity**, with xkain an established active alternative. linuxrrze is a much smaller but very recent update-focused fork. This is a research ranking, not a tested migration recommendation.

### Cited Findings
- Snapshot: 2026-11-03 America/Los_Angeles (GitHub timestamps below are UTC). Enumerated **154 direct forks** of rstrouse/ESPSomfy-RTS, then searched GitHub for separately named and secondary-fork derivatives. Excluded SidShaytay's newly created fork from candidate recommendations. — [Fork enumeration API](https://api.github.com/repos/rstrouse/ESPSomfy-RTS/forks?sort=newest&per_page=100); [GitHub search](https://github.com/search?q=ESPSomfy+fork%3Atrue&type=repositories)
- **cjkas**: default main is **399 commits ahead, 0 behind** original upstream. Latest stable release is **v4.0.2, 2026-11-01**, following v4.0.1 and v4.0.0 on October 29. **304 default-branch commits since 2026-08-05 UTC**, including merge, documentation, version and CI commits; 297 attributed to cjkas, 5 GitHub Actions bot, 1 Dependabot, 1 Ropheb. At least **77 merged PRs in this period** among the latest 100 PRs retrieved; 3 open. Actual code examples include GPIO/boot-strapping warnings and ESP32-P4 work, not only repo syncing. — [Upstream comparison](https://github.com/rstrouse/ESPSomfy-RTS/compare/main...cjkas:ESPSomfy-RTS:main); [Releases](https://github.com/cjkas/ESPSomfy-RTS/releases); [v4.0.2](https://github.com/cjkas/ESPSomfy-RTS/releases/tag/v4.0.2); [Commits since Aug 5](https://api.github.com/repos/cjkas/ESPSomfy-RTS/commits?since=2026-08-05T00:00:00Z&per_page=100); [Pull requests](https://github.com/cjkas/ESPSomfy-RTS/pulls?q=is%3Apr+is%3Amerged); [Recent code commit](https://github.com/cjkas/ESPSomfy-RTS/commit/8a59f511)
- **xkain**: main **194 ahead, 0 behind** upstream. Latest stable **v2.6.1, October 17**; earlier v2.6.0 October 10, v2.5.9 September 9, v2.5.8 September 5 and v2.5.7 August 30. **37 main commits since August 5**, authored by xkain (35) and jcrespoc (2), and **10 merged PRs** in that period. Main last substantive change November 2 fixes timer rollover; other November 2 work fixes backup/settings-record parsing. Main says v2.6.2, which has not yet been published as a release in the retrieved data. — [Comparison](https://github.com/rstrouse/ESPSomfy-RTS/compare/main...xkain:ESPSomfy-RTS:main); [Releases](https://github.com/xkain/ESPSomfy-RTS/releases); [Commits](https://github.com/xkain/ESPSomfy-RTS/commits/main/); [Timer fix](https://github.com/xkain/ESPSomfy-RTS/commit/f55fb4cc); [Backup restore fix](https://github.com/xkain/ESPSomfy-RTS/commit/5e09cd46)
- **linuxrrze**: main **66 ahead, 0 behind**, all 66 main commits fall since August 5 (55 Marcel Ritter, 10 Actions bot, 1 Evan Broder); no PRs retrieved. Latest **v2.4.19 November 2**. Many increments, committed binaries and releases were concentrated on November 2, so the 66 should not be read as 66 independent features. Actual edits include GitHub release cache timestamps, cache-control handling and update-page improvements. — [Comparison](https://github.com/rstrouse/ESPSomfy-RTS/compare/main...linuxrrze:ESPSomfy-RTS:main); [Releases](https://github.com/linuxrrze/ESPSomfy-RTS/releases); [GitHub timestamp code](https://github.com/linuxrrze/ESPSomfy-RTS/commit/2a813cfc); [Cache code](https://github.com/linuxrrze/ESPSomfy-RTS/commit/9cdd02de)
- Popularity is separate from activity: xkain had **54 stars**, cjkas **38**, linuxrrze **0** at retrieval. Both larger forks are mainly single-maintainer projects by recent authorship despite additional contributors. — [xkain API](https://api.github.com/repos/xkain/ESPSomfy-RTS); [cjkas API](https://api.github.com/repos/cjkas/ESPSomfy-RTS); [linuxrrze API](https://api.github.com/repos/linuxrrze/ESPSomfy-RTS)

### Inferences
- cjkas is the best initial target for a maintained baseline/updater investigation, given measured main-branch volume, PR workflow and recent stable releases. Volume does not establish correctness, security or LILYGO compatibility.
- xkain is also clearly maintained and may offer useful smaller fixes or a different migration path. Its longer release sequence is stronger evidence than its star count.
- linuxrrze deserves targeted cherry-pick inspection for the screenshot/version/update questions, but its burst of releases is not evidence of broad community maintenance.

### Gaps
- Counts include merges, docs, version bumps, binaries and bot work. I did not compute normalized changed-code volume or defect rates. PR counts cover the latest 100 records, so cjkas's count is a lower bound if older updated PRs were omitted.
- No hardware, radio, motor, HA or migration tests were performed. Board-specific feature verification belongs to the separate existing-solutions researcher.

## Are there other active derivatives?

### Takeaway
Yes: a smaller secondary fork, Pulpyyyy, has recent beta development; several specialized branches exist. A recent pushed_at or a name such as Matter/Ultra is not proof of current code development.

### Cited Findings
- **Pulpyyyy/ESPSomfy-RTS**, secondary fork of xkain: default main's latest commit is October 6 (v4.1.0), but releases **v4.2.0-beta.5 November 3 UTC** and beta.3 November 2 show current non-main/tagged development. Beta.5's tagged commit is b7ceb408, November 3 06:28 UTC (November 2 local). Stable main has 40 commits since August 5; therefore default-main counts alone understate this project's ongoing beta activity. — [Releases](https://github.com/Pulpyyyy/ESPSomfy-RTS/releases); [Beta 5](https://github.com/Pulpyyyy/ESPSomfy-RTS/releases/tag/v4.2.0-beta.5); [Tagged commit](https://github.com/Pulpyyyy/ESPSomfy-RTS/commit/b7ceb408730b5779093d2a4c1c73e1153c1d4aa3); [Main](https://github.com/Pulpyyyy/ESPSomfy-RTS/commits/main/)
- Pulpyyyy's beta/radio-queue branch contains a substantive October 7 fix changing RF orders into contiguous frame trains, and listen-before-talk quiet-channel behavior fixes. Those are behavioral changes requiring validation, not automatic improvements to deploy. — [Contiguous train fix](https://github.com/Pulpyyyy/ESPSomfy-RTS/commit/fce5a989f29efdac69c8b93691b5cabc2c5a3da7); [LBT fix](https://github.com/Pulpyyyy/ESPSomfy-RTS/commit/180925811146ea8803cb240e4f76af596c9bb11e)
- **mosch** is 6 commits ahead of upstream, with an actual November 1 commit spacing Matter commands and October 28 changes exposing groups and always sending open/close; no GitHub releases retrieved. — [Commits](https://github.com/mosch/ESPSomfy-RTS/commits/main/)
- **ebroder** is 7 commits ahead; last October 20 drops blocked websocket clients; September work adds delivery logs and MQTT events. No releases retrieved. — [Commits](https://github.com/ebroder/ESPSomfy-RTS/commits/main/)
- **Moritz-Staat** is 12 ahead; v2.4.9 September 6, latest code September 26 persists sorting. — [Releases](https://github.com/Moritz-Staat/ESPSomfy-RTS/releases); [Commits](https://github.com/Moritz-Staat/ESPSomfy-RTS/commits/main/)
- **Kirbyrc/ESPSomfy-RTS-PIO** has substantive September 2026 PlatformIO/pin work but no releases retrieved. — [Repository](https://github.com/Kirbyrc/ESPSomfy-RTS-PIO)
- **Nabiroo**, **gh-residante** and **cmeron** have recent repository pushes but their main histories compare identical to upstream. gh-residante publishes an October 24 release tagged v2.4.6; that is not evidence of newer code. **forbon/ESPSomfy-RTS-Matter** and **Pypypierre/ESPSomfy-RTS-Ultra** also have upstream-identical default main histories at retrieval. — [Nabiroo comparison](https://github.com/rstrouse/ESPSomfy-RTS/compare/main...Nabiroo:ESPSomfy-RTS:main); [gh-residante](https://github.com/gh-residante/ESPSomfy-RTS); [forbon comparison](https://github.com/rstrouse/ESPSomfy-RTS/compare/main...forbon:ESPSomfy-RTS-Matter:main); [Pypypierre comparison](https://github.com/rstrouse/ESPSomfy-RTS/compare/main...Pypypierre:ESPSomfy-RTS-Ultra:main)

### Inferences
- Pulpyyyy should be called out as an active beta derivative rather than silently omitted or ranked solely by main. Its latest release being a prerelease is material to a bedtime/working-controller migration decision.
- Several specialized forks offer isolated fixes worth inspecting without wholesale migration.

### Gaps
- This survey systematically enumerated 154 direct forks and searched secondary/renamed derivatives, but it is not an exhaustive review of every branch of every descendant. I cannot guarantee no obscure fork is busier.
- forbon/Pypypierre may have non-default branches; identical main does not mean the entire repository has no experiments.

## Which evidence should drive a migration choice?

### Takeaway
Inspect cjkas and xkain first; compare actual board support, version/OTA handling, calibration/counter/backup schema and matching HA integrations before choosing. Do not infer that a generic S3 binary works on LILYGO Plus.

### Cited Findings
- cjkas's November 1 ADR title explicitly covers Home Assistant checking GitHub releases itself, and October 31 changes cover release provenance/pinning/scanning. These are directly relevant to versioning/update concerns, though the separate solutions researcher should establish exact supported behavior. — [ADR commit](https://github.com/cjkas/ESPSomfy-RTS/commit/e3ea95c5); [Supply chain commit](https://github.com/cjkas/ESPSomfy-RTS/commit/5c57d1a2)
- xkain's November 2 main fixes include restoring general settings and radio from backups and settings-record sizing; they intersect this user's backup/calibration requirements. — [Restore fix](https://github.com/xkain/ESPSomfy-RTS/commit/5e09cd46); [Record size fix](https://github.com/xkain/ESPSomfy-RTS/commit/d1216e76)

### Inferences
- Before bumping or building a new private OTA scheme, reuse these maintained projects' relevant designs where verified. Preserve our hardware profile and current live configuration; do not flash or issue motor commands during research.

### Gaps
- Exact LILYGO Plus compatibility and safe board-asset selection were not validated in this activity-ranking subtask.

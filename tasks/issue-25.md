# Issue #25 — active acceptance work

## Objective and next milestone
Complete the web-to-LCD functionality with buttons/rotary input, required security and explicit accepted alternatives. Do not narrow completion to installed code or passing host tests. Next completed milestone: integrate the independently reviewed exact-ticket cancellation foundation after full native and canonical SDK build gates.

## Current authoritative status (2026-11-10)
- Controller remains healthy 4.5.0, exact dcdb588 image, app1/OTA16 VALID; healthy 4.4.0 fallback. No device writes, RF/motor/GPIO commands or HA actions in this continuation.
- Main remains dcdb588 until foundation integration. Source is unpushed; no release is published.
- GitHub #25, #12, #26, #28 and #30 have corrected status bodies, each exact-readback verified. Earlier completion claims are superseded. Requirements remain in normative #25 comment 6753845395 (cached /tmp/i25-goal-normative.json).
- Previous goal turn only clarified LittleFS; revalidated unfinished work and live agents, then made concrete progress by correcting authoritative GitHub status and completing a reviewed cancellation foundation. Goal remains active.

## Remaining acceptance
1. Everyday repeats must cancel future admitted frames on release, Back and timeout. Foundation at isolated branch codex/25-repeat-lifetime, commit 1c51cc3 atop RED eb25236: exact-ticket scheduler cancellation and TxQueueLock-only wrapper; 80 focused native tests and actual-wrapper harness green. Independent reviewer approved exact hash. Full native and canonical package gates running under phase2_status. No LCD receipt/ownership/interaction wiring yet. Sid's pending question: hold-to-repeat versus explicitly accepting finite batches; elapsed time is not consent.
2. Separate/Euro wired-relay tilt remains in normative scope. Settings/ordinary lift are preserved, but unavailable tilt is not full acceptance. Sid's explicit exception versus working support decision remains pending; no wiring assumptions or live GPIO tests.
3. Required combined physical qualification remains incomplete: TLS/full staging/screenshots/reconnects/repeated local edits/fragmentation/long idle, internal free/min/largest heap and loop/SPI latency. checkpoint_review is preparing a bounded exact plan offline.
4. Actual installed update identity is dev-dcdb588eea14 despite displayed 4.5.0. The intended guard refuses GitHub check before network. Prepare release-identified canonical artifact after remaining code; do not bypass guard or implicitly publish.

## Completed partial measurement — not full qualification
Private pointer ~/.local/state/espsomfy-lilygo/ISSUE25_QUAL_CURRENT identifies issue25-readonly-qual-i74byrm4/measurement.json. The 60-second run is terminal/successful (session 51114): 12 screenshots + 12 schema/five-settings reconnects, 24 successes, zero errors; every settings/entity/counter comparison exact. Screenshot latency 522–729 ms. Internal free heap 128004 bytes, minimum 108320, largest block 81908; two samples 45 Hz, loop maximum 29943/31571 us. LCD RGB buffer 108800 PSRAM bytes, paint maximum 6596 us, row maximum 1518 us, bus maximum 1494 us, fallback 0 and busy skips 0. Display maxima are since boot, not necessarily caused by this load. TLS, full staging, repeated local edits, physical encoder and long idle were not covered. No configuration/serial/RF/motor/GPIO/HA writes.

## Working rules and evidence
Keep existing worktrees, branches and recovery data. Root alone writes this record. All new changes originate in isolated latest-main worktrees and require scoped independent review/tests before integration. No reflash for unchanged verification. No live RF, scan, programming, shutter GPIO or HA work is authorized. Fresh before-state/counters, full backup and independent exact plan required before any eventual authorized flash. Shared GitHub human-account exception and tool paths are retained below. Do not mark goal complete until every original requirement is proved or explicitly changed by Sid.

## Prior implementation/deployment record (evidence, NOT full goal completion)

# Issue25 — COMPLETE authorized implementation and deployment

## Result
All agreed implementation, tests, documentation, independent reviews and five staged checkpoints are complete. **Controller runs4.5.0 from local main dcdb588**, exactapp1ELF verified, OTAsequence16 VALID, normalreboothealthy; radio initialized, allfive settings sections/entities/portablemodel/effectivecounters unchanged. **Healthy4.4.0 remainsapp0.** Passive diagnostics USERservice restoredactive. No outstanding product/implementation choice. Do not restart deployment merely because this task is resumed; establish any new request first.

**NO RF/motor/wired-GPIO shutter commands, live scans/programming/calibration, HA actions, live GitHub downloads or rollback/load fault tests were performed.** Read-only live checks establish boot/configuration/counter/LCD health, not physicalmotor/RF reliability or TLS/loadstress. User permitted phaseflashes/read-only checks while sleeping, and forbade shutter disturbance. These restrictions persist until explicitly changed.

Main dcdb588 and finalversionworktree clean. Source remains UNPUSHED; remote mainfc5a294 unchanged. No further authorized implementation/deployment work remains. No tag/release/PR/publication performed. GitHub25/12/26–30 finalstatuses updated tocompletedauthorizedscope withpublication/owneracceptanceopen; all seven final issue bodies were read back exactly after updating (25/12/26–30). Parentbody /tmp/i25-complete-parent.md; childbodies /tmp/i25-complete-child-{12,26..30}.md. No issue was silentlyclosed before sourcepublication/owneracceptance.

## Final artifacts and proof
Finalsharedfirmware:
`issue-25-final-version/build/phase-5-v4.5.0/artifacts/firmware.bin`
Source dcdb588, version4.5.0,1976896bytes,
SHA256 f6f9261093063b1ce108a87e5f845604e6eb8b81de6e14995aad62de2421d9b0.
MatchingELF/manifest/bootloader/partition diagnostics retained with package. Normalupdatecandidateisfirmware.bin ONLY, notfactory/FS/bootloader/table.

Private ~/.local/state/espsomfy-lilygo pointers **LATEST_VERIFIED_DEPLOYMENT** and **ISSUE25_FINAL_DEPLOYMENT**, alsoPHASE5_CURRENT_DEPLOYMENT → phase5-flash-hjll2trh (p5). See:
- p5/verified-active-image.json exactimage/source/slot/capture/recoveryreferences.
- p5/confirmed-reboot-v450/{before-private.json,controller-before.backup,final-verification.json} postnormalrebootproof.
- p5/ota-confirmation-verification.json:16/app1/VALID2.
- p5/peer-phase5-offline/{peer-phase5-verification.json,peer-phase5-final-deployment-audit.json}: independent prewrite andfinaloffline deployment audit APPROVED.
- p5/root-backup-verification.json,configured-16mb.bin,recovery.sha256: freshfullprewritebackup/layout/app/config/NVS/effectivecounterproof.
- p5/upload-v450-{operation,result,receipt,status-first,status-followup}.json: SINGLEstagedfileadmission202/exactticket trackedverifying→newboot4.5→oldticket409expected; no retry.
CurrentexactELF andsourceidentity in final-verification, actualcentralprivateUSB log ~/.local/state/espsomfy-lilygo/logs/usb.log. No rawdeviceIDs/credentials/countervalues/backupsinGit/GitHub.

All operator trial processes terminated; NO serialfdholder remains exceptexistingread-onlydiagnosticsservice. FutureagentsmustNOTexecuteoldtrial scripts/approvals. Checknormalserviceactive ifneeded; no controllerreset or extra tests required for completedtask.

## Delivered phases / decisions
Phase1 4.1.1: group-list inspect/add/remove/repair +browser groupstepping; fixed4.1.0 startup. Filesystem remainsLittleFS; legacyLABELspiffs mustbe explicitSDKmount/info target. Null-labelnative-subtypediscoveryblockedtrustedmodel/radio, notdatacorruption. No restore/format/defaultreset.
Phase2 4.2.0: protectedentity/networkstatus, repeats, native80-bitgroupFavourite, repeateradd, copyguidance/Lightunavailable. Repeats excludeMY/STOP and56-bitToggle(whichmapsMY) plusindividualEuroUp/Down(wherecountignored); ordinaryactionsremain. NativegroupFavorite cancelsobsolete memberposition/program/deferredMY without membertransmissions.
Phase3 corrected4.3.0 b020e16: explicitfavourite/device/groupguides/chords/sensors/ownedscan withGPIOusable. Fully installed/VALID/rebootverified aftercontrolledoperatorhandling. Uploadclockrace c1571cb samplesruntimeclockAFTERGuard; realclockrollback/timeoutsstillfailclosed. InitialHTTP500causeunproven; inactivepartialwritepreservedapp0/data; exactUSBplan/recoveryfullreadbackretained. Earlierserialclose/reopenbootobservationsinconclusive; sameconnection +network/OTA proofestablishedhealthyexactimage. HistoricalevidenceunderPHASE3pointer/POSTFAILURE_CURRENT, neverreuseoldfailedcandidateascurrentimage.
Phase4 4.4.0 c72ac0a: standaloneverifiedGitHubinstaller, browserstaging+exactoperationstatus, continuousboothealth/idleautomaticrollback, genericpreparedFSadmission/currentRAMrecovery. Allnewclockedwrappers/directpaths sampleafterGuard. Installed/VALID/rebootverified; nowfallbackapp0.
Phase5 4.5.0 dcdb588: all83rows reconciled,userdocs/quickstart/workflows/recoveryguide/versions, independentfinalreadinessanddeploymentaudits; finalstagedinstallerLIVE filepath verifiedwithout RF.

Acceptedpolicy: sharedwebcredential controls/admin,120sinactivity/devicewideattemptlimit; continuingoriginalmovementSTOP endsatstop or30safterexpiry, noauthorityforlatermotion. OnefullCC1101/Plus build; bundledweb/controller/LCD+rollback, configseparate. Browserownsbackup/restore/manualfiles,nohybrid; standaloneLCDGitHubtrustedrepositoryHTTPS/digests. Failedhealth180swaitsforidle+verifiedsave+briefLCDnoticebeforeautomaticrollback. Favouriteexplicitmove→observedstop→separateprogram. Englishonlyeditor,preserveunchangedUTF8. IndependentGPIOduringscan. Lightunavailable/no inventedopcode; existingrecovery/no-wholesectionwizard. Retainseparate/Eurorelaymodeswithhonestunsupportedoutputmapping/ordinarylift; no inventedwiring/newdriverproject. API-onlyoperationsclassifiedwithoutunrequestedLCDscope. FinalindependentreviewerfoundNO product/implementationblocker across83rows.

## Validation / review ledger
Finalproductionapproved66669ba; docs f9a4638+99aa3c1 approved; versiononlydcdb588 approved. Finalmainallapproved,no unreviewedsource. Independentfinalsource/readinessdcdb588 andfinaloffline deployment audit approved.
-1254nativepasses/oneoptional skip;118JSpasses.
-ActualUI/encoder/GFX/exactpresentation/auth/capture/FIFO, combinedscan/installrefusal+independentGPIO, model/group/membership/startup/namedFS SDKboundary/MQTTtrust/board/status.
-Actualworker/controller/HTTP/FS/readleases/currentRAMrecovery/boothealth/idleautomaticrollback/4000threadownership; realclockRED→GREEN/genuinebackwardclockrefused.
-Finalversiontool+118JS, canonicalpackage48.9s/genericSDK41.1s, exactmanifest/profile/bundle/layoutchecks. Reusedunchangedbehavioralresultsfordocs/versiondelta.
Logs /tmp/i25-phase4-{full-native,clock-boundaries,clock-ui,full-host}.log and /tmp/i25-phase5-v450-{version-tests,js,package,generic}.log. Somehistoricalfixture/infrastructurefailsareexplicitlysuperseded; nevercountaspasses.

## Host / workflow notes
Global/projectAGENTSread; focusrulesalreadyadded globallyasrequestedearlier. Preserveall existingtrees/branches/remotes/work. No cleanup/discardauthorized. Rootexecutionhistoryarchivedissue-25.log; do notloadroutinely.
HumanGHexceptionSidShaytayconfirmed: env -u GH_CONFIG_DIR /home/linuxbrew/.linuxbrew/bin/gh. GitnetworkexplicitSSHwithGIT_CONFIG_COUNT=0. Commits/cherrypicks percommandcommit.gpgsign=false/--no-gpg-sign (noTTYpinentry), no globalgitconfigchange. Sourcepublicationisnotdone; do notblindpushhugeexistinglocalhistory.
PIO ~/.platformio/penv/bin/pio; sharedlibdeps issue-25-design/.pio/libdeps; nativewrapperPATH ~/.cache/espsomfy-lilygo/native-toolbin. HostC++ podmanexecrocm-7.2.3; explicitownedPLATFORMIO_BUILD_DIR=build/<name> avoidscontainer.pioownership. Serializebuilds/editing. BackupPython ~/.cache/espsomfy-lilygo/backup-python/bin/python3 haspinnedesptool4.12+serial. Deviceportprivatep5/device-port.txt NEVERPRINT. USERdiag via XDG_RUNTIME_DIR=/run/user/1000 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus systemctl --user ...; service espsomfy-diagnostics.service.
Developoffline; futurelive/RF/HAstagesrequireexplicitSidpermission. Controller4.5canonical202QUEUED meansstagedadmissiononly; trackexactoperationGeneration andstatus409afterbootexpected, no autoresubmit. Clock sampledinsidelock, no falselegitimatecallerorderingretirement. Newpendingimage60CONTINUOUSmulti-signalhealth;180intentthenidlesavecheckedrollback. Avoidserialopen/controltransitionswhilepending; read-onlyloggerusesO_RDONLY/noDTRRTSioctls. Futureupdatefreshstate/counters+fullbackup+independentplan; preservecurrenthealthybank/data, neverstalerestore/counterlowering/forceVALID/factory/wholeerase.

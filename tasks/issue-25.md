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

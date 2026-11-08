# Saving model changes

Room, device and group edits change the running model once. The reply separately reports whether their model file was saved. An assigned ID remains valid even if the subsequent file save fails. The browser adopts that ID, or closes the completed delete workflow, before showing a persistence warning.

**Retry Commit** writes the current running model. It does not repeat the original add, delete, link, pairing action or RF command, and it does not consume or rewrite NVS rolling codes. After a failed or uncertain save, retry Commit before restarting; a restart may otherwise return to an older configuration.

A successful model commit writes a separate `/shades.cfg.new`, checks write/flush/sync/close results, compares the staged bytes against the stable running model, replaces `/shades.cfg`, and verifies its bytes again. The old file stays intact until the verified replacement is ready. The source model and its counters are not changed by serialization. Only a verified model-file result clears the dirty flag. Failed attempts are throttled like successful writes; the existing automatic dirty-model retry also reports its result.

If an operation fails before replacement, the previous file remains. If replacement succeeds but verification afterward fails, the result is **uncertain**. The implementation keeps the running model dirty and reports the uncertainty; it does not attempt to roll back the file or counters. These are checked filesystem results and readback, not a tested guarantee for every reset, flash fault or power interruption. No controller fault-injection or flashing was performed during offline development.

The record format and existing load-time maximum-of-file/NVS rolling-code policy are unchanged. Non-tilting record normalization uses bounded copies. An edited shade normalizes its live tilt state during its Save action; a commit-only retry does not normalize or publish that edit again.

## Browser application uploads

Application-file transfer remains browser-owned. HTTP 202 means the image upload completed and a model save was queued; it does not mean the model was saved. The loop reports the persistence outcome over `modelPersistence`. A successful upload requests an automatic restart only after its own queued model save is verified and no operations remain queued.

A queue-full response says the image was uploaded but the model save was not queued. An upload error remains an error. A failed or uncertain save, abandoned upload or failed queue admission does not schedule an automatic restart. Use Retry Commit, then explicitly Restart after saving. A later successful retry does not inherit an earlier failed request's restart permission.

Only application-upload recovery may go ahead of a held RF queue head. It saves the current model without running or removing queued commands or edits. If operations remain queued after the recovery marker is removed, the result says **Model file saved; restart not scheduled** (`rebooting` is 0). Resolve those operations, then explicitly Restart. Ordinary Commit retries keep FIFO order. Recovery does not freeze new submissions or replace coordinated manual-upload admission; that admission must prevent or drain pending work before stopping the radio.

This does not make a whole-filesystem image upload transactional. Browser backup/restore remains a separate existing workflow; its pre-existing aggregate completion reporting is not upgraded to transactional or rollback guarantees here.

## Integration

`SomfyShadeController::commit()` returns `ModelCommitResult` (`saved`, `failed` or `uncertain`, plus the failure stage). Entity saves/deletes propagate that result. `applyModelEdit()` is the loop-owned shared edit seam: it applies once, attempts persistence, sends an optional socket reply, and fans out the actual running changes even when persistence fails. Local callers leave `replyTo` empty.

An applied edit has `applied=true`; `ok` and `durable` certify the verified **model file** only. They do not certify independent NVS writes. Retry through the controller COMMIT or `submitModelCommit()`, never by replaying `applyModelEdit()`. HTTP and socket tasks queue commits so serialization runs on the model-owning loop. The bounded serializer emits one record at a time and verifies exact bytes without copying the full model.

Explicit linked-remote rolling-code updates still use the pre-existing unchecked NVS setter. The model-file result does not qualify that separate write. A standalone linked-counter or correction administration flow requires a checked explicit counter operation and typed failure reporting before exposure; a file Commit must not be used to replay or roll back that counter operation. Ordinary on-air rolling-code behavior remains outside this change.

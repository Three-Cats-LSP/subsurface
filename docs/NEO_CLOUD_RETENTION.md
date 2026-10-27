# Cloud snapshot retention and cleanup design

Design and read-only Windows audit, 2026-10-17. [Native storage review](NEO_CLOUD_STORAGE_REVIEW.md) is implemented and Windows-validated at `ebaf24069`; [experimental coordinated history](NEO_CLOUD_HISTORY.md) and its later [local authority](NEO_CLOUD_AUTHORITY.md), [branch review](NEO_CLOUD_BRANCH_REVIEW.md) and [slow-transfer](NEO_CLOUD_STALL_TESTING.md) increments are implemented and Windows-validated. Exact revisions and completed latest QML package checks are recorded in those evidence files. Normal synchronization still uses the [schema 2 commit protocol](NEO_CLOUD_COMMIT_PROTOCOL.md). The separate development history copies data without migrating ordinary sync or enabling deletion. Milestone 13 remains in progress. Physical Android validation and two-live-account Dropbox testing remain deferred.

## Decision for existing logs

Keep all existing schema 1/2 files. Add a read-only storage review before implementing cleanup. An unreferenced filename is not evidence that a snapshot is disposable, and neither file age nor a successful sync supplies that evidence.

`CloudSyncManifest` stores the current payload, its hash, a parent hash and a client-generated timestamp. It has no complete commit history, outstanding-upload registry, reader reservations or retirement records. `CloudSyncManager::startUploadSequence()` creates a snapshot before publishing its manifest. `CloudSyncFileStore` has no deletion API. Those are deliberate limits on what a cleanup implementation can infer from the current format.

| Existing object | Retention rule |
| --- | --- |
| Active manifest and its exact payload | Always protect; a missing, ambiguous or invalid reference blocks cleanup |
| Recovered initial manifests and their payloads | Protect every recovery branch; several manifests may reference the same payload |
| Legacy `subsurface-neo.xml` | Preserve as a migration/recovery copy |
| UUID snapshots without a known reference | Label commit status unknown; preserve, including possible interrupted or losing uploads |
| Unknown names, unsupported manifests, folders or incomplete metadata | Preserve; never infer ownership from a prefix alone |
| Files in another account, provider or cloud namespace | Outside this review and any cleanup plan |

Account identity must be verified before inventory or file access. Local `--user` profiles do not isolate remote storage. Primary and Backup providers require separate reviews; a copy on another provider does not authorize deleting this provider's copy. Retention covers canonical XML only, not the separately stored equipment library, manual collections or planner presets.

## Why a sweep cannot be added to schema 2

The following are source-derived interleavings, not newly executed native tests:

| Sequence | Failure from deleting an apparently unused snapshot |
| --- | --- |
| Writer uploads P, pauses before publishing; collector sees head H and deletes P; writer conditionally replaces unchanged H with a reference to P | The current committed manifest points to a missing payload. Re-reading H before deletion does not prevent this race. |
| Reader captures manifest P; another writer publishes Q; collector deletes P before the first reader downloads it | The pending download fails. Cleanup needs reader coordination or an explicit safe re-read/retry contract. |
| A manifest update succeeds but its acknowledgement is lost; the writer treats its upload as abandoned | Deleting that upload can delete the current cloud log. An error or cancellation is not proof of non-commit. |
| Initial Google recovery archives a competing manifest; collector examines only the active head | The archived branch loses its recovery payload. |
| A process resumes after any fixed grace interval | Time alone does not fence its pending publish. The current network timeout is an inactivity limit, not a maximum lifetime for a suspended client. |
| An older client ignores new cleanup metadata or rewrites the legacy manifest | A sidecar cleanup file cannot enforce a protocol the writer does not follow. |

`parent_sha256` is a content hash, not a unique predecessor file ID or a complete history. A UUID filename and provider modification date cannot distinguish an old successful commit from an abandoned attempt. Do not display the unknown category as reclaimable space.

## Target policy for a future coordinated format

The following are initial design defaults, not active settings or an implemented wire format:

- Cleanup starts disabled. The first product increment is storage review with no delete control.
- Once the protocol and restore path below pass validation, opt-in cleanup retains the current log, at least the latest **20 confirmed committed versions**, and **every confirmed version from the previous 30 days**. These are a union: a version is eligible for retirement only when it is outside both the count and age windows.
- User-pinned versions, unresolved conflicts, recovered initial branches, migration copies and unknown files stay protected independently of those windows. Restoring a version publishes a new snapshot with a new identity; it does not repoint the head to a retired object.
- A positively identified abandoned upload can enter retirement only after its publish permission has been removed atomically. A lost response, old timestamp, missing local process or missing local credentials is insufficient.
- Retired objects remain physically available for at least **30 further days** before deletion. This is an extra recovery interval, not the concurrency safeguard. Missing or unreliable time evidence extends retention; it never accelerates deletion. Client `created_at` is not an authoritative age source.
- Storage pressure reports an actionable error and preserves local data. It never shortens the windows or deletes protected versions to make an upload fit. These rules therefore do not promise a hard storage cap.

Committed order must come from the coordinated commit record. Time-based decisions need validated provider time evidence associated with those commits and retirement transitions, with conservative clock-skew handling. Exact time acquisition and persistence are implementation gates. Provider file creation time by itself is not proof of commit time or abandonment.

## Required protocol before deletion

Use a new versioned namespace and a single authoritative control record for that namespace. The record must atomically coordinate the current head, retained history, explicit pins/recovery roots, registered upload attempts and retired objects. A collection plan in a separately updated sidecar cannot make publication safe.

1. **Establish one root.** Bind it to the verified provider/account and a unique namespace epoch. Google duplicate creation must converge before any cleanup can begin; ambiguity blocks cleanup. Keep schema 1/2 namespaces and every object created under them outside the deletion set. A migration copies a verified log into newly named objects; it does not make old objects collectible. Older clients must not be able to overwrite the new root or refer to objects eligible for its collector. Legacy-namespace changes must be detected and surfaced as a separate branch, never silently merged or erased. Initial setup, migration and rollback need their own compatibility tests.
2. **Register before upload.** A writer conditionally adds a unique upload ticket to the control record, including its namespace, never-reused attempt identity and intended payload name. Creation uses a new object identity. Upload completion records the exact provider object ID/revision, size and checksum. A crash before registration cannot create a legitimate publishable object.
3. **Publish through that same record.** After validating payload completion and unchanged local data, the writer conditionally updates the root only while its ticket is active. The update atomically installs the new head, records its confirmed history and consumes the ticket. All writers preserve concurrent retention state when re-reading after a revision conflict. A retired or missing ticket cannot be revived; retry allocates a new ticket and snapshot. A lost response is resolved by re-reading the root, never by deleting the upload.
4. **Retire atomically.** After a complete review, the collector conditionally marks eligible objects retired and removes their publish permission in the same root update. It must not retire the head, retained history, pinned objects or unresolved recovery roots. A concurrent writer/collector update invalidates that plan and requires a fresh read. Explicit abandonment and expiry also use this conditional transition. No object deletion precedes confirmed retirement.
5. **Coordinate readers and restore.** The initial implementation must register a read/restore reservation through the same root before accessing an object that could be retired. Retirement excludes reserved objects; a reservation obtained during the retirement interval blocks deletion. A stalled reservation is retained; automatic expiry is out of scope until safe revalidation and retry are implemented. A process that lost a reservation must re-read and reacquire before applying data. Keep checksum/XML validation and the existing local-edit guard. Process failure may leak a reservation, which costs space rather than data.
6. **Fence deletion after the recovery interval.** Re-verify account/namespace and exact object identity/revision, then conditionally advance the object from `retired` to `deleting` in the same root, only while no pin or reservation protects it. Once `deleting`, no new reservation, pin or publication may reference that identity. This transition must race atomically with new readers; a read-then-delete check is insufficient. Delete only that immutable object with the provider's verified conditional operation, then record `deleted`. Concurrent collectors must be idempotent. A revision mismatch, lost response, permission failure or ambiguous lookup stops progress for that object; retry inspects exact identity rather than widening the deletion target. Provider 404 alone cannot establish successful cleanup unless identity and authorization are independently established. After an uncertain delete response, never revert to a state that admits new readers.
7. **Keep the fence durable.** A restarted writer, collector or reader must not resurrect retired identities. Never reuse attempt IDs, object names or namespace epochs. Retain retirement records until a separately validated compaction protocol preserves that guarantee and interrupted-delete recovery. Bound control-record growth by pausing new work with a clear error when necessary; do not silently discard safety metadata.

This is a protocol design, not a proof that the existing clients meet it. Conditional writes are necessary but do not themselves validate ticket, history or reservation transitions. The state machine, provider operations and compatibility boundaries must be implemented and tested together. An implementation may simplify this design only with equivalent demonstrated guarantees.

## Provider requirements and read-only review

Google App Data does not support trashing files, and `files.delete` permanently deletes them. A Google cleanup feature must therefore describe permanent deletion accurately and must not promise a recoverable Trash stage. The recovery interval above leaves the original object in place. The [2026-10-27 disposable deletion probes](NEO_CLOUD_DELETE_PROBES.md) separately verified stale-ETag rejection with v2 DELETE. The v3 DELETE probe ignored that stale precondition and deleted its disposable target, so it is unsuitable for this guarded operation. A native deletion adapter and the full coordinated protocol still need implementation/validation; deletion remains disabled. See [App Data constraints](https://developers.google.com/workspace/drive/api/guides/appdata#constraints_on_the_application_data_folder) and [v2 files.delete](https://developers.google.com/workspace/drive/api/reference/rest/v2/files/delete).

Dropbox exposes `DeleteArg.parent_rev` to condition deletion on the latest file revision. The [disposable probes](NEO_CLOUD_DELETE_PROBES.md) verified stale-revision rejection, exact-ID deletion/retry and a throttled competing request. A future native adapter must preserve its deletion fence across errors and uncertain responses. Do not use directory deletion or depend on provider restore windows as Neo's retention guarantee. See [Dropbox DeleteArg](https://docs.dropboxapi.com/dropbox-api/api-reference/user-endpoints/files/delete-v-2).

The first native feature should show counts and bytes for current, recovery, legacy, unknown-status and other protected objects. It must collect every Google `nextPageToken`, reject `incompleteSearch`, and exhaust Dropbox `has_more` through its continuation cursor. Invalid/repeated cursors, duplicate identities, unreadable manifests, unknown schema, missing reference targets, account changes or request failures produce an incomplete review with no deletion plan. Re-read the control state after scanning; a changed root invalidates the review. Pagination and two equal scans are useful observations, not a cross-file transaction or a publication fence. See [Google files.list](https://developers.google.com/workspace/drive/api/reference/rest/v3/files/list) and [Dropbox listing/continuation](https://dropbox-sdk-python.readthedocs.io/en/latest/api/dropbox.html#dropbox.dropbox_client.Dropbox.files_list_folder).

The storage review must not invoke the existing sync lookup as a supposedly read-only scan: Google lookup can rename competing initial manifests during recovery. Use a separate read-only inventory path. Never download unrelated file contents just because their names share a prefix. Keep account identifiers, tokens, client JSON and dive contents out of diagnostics.

## Windows read-only inventory, 2026-10-17

The two existing authorized disposable profiles were audited without launching Neo or modifying cloud files. Each audit verified the stable account identity, exhausted provider pagination, downloaded the active manifest/current payload, inspected recognized recovery-manifest references and repeated the inventory. The current manifest/payload IDs and bytes matched the earlier verified baseline. Original local credential and sync-state records remained byte-identical. No cloud mutation request was sent; token renewal occurred only in memory.

| Category | Google Drive | Dropbox |
| --- | ---: | ---: |
| Active manifest | 1 | 1 |
| Current payload | 1 | 1 |
| Legacy payload | 1 | 1 |
| Recognized recovery manifests | 25 | 0 |
| Additional payloads referenced by those manifests | 4 | 0 |
| UUID snapshots with unknown commit status | 10 | 6 |
| Other entries, preserved without content inspection | 5 | 0 |
| Total inventory entries | 47 | 9 |
| Eligible for automatic deletion under schema 2 | **0** | **0** |

Each provider returned one inventory page on both scans. No recognized recovery manifest was malformed, and each referenced payload resolved to one inventory object. Recovery payload bodies were not downloaded or validated; reference resolution is not a restore test. Google unknown-status snapshots occupied 3,521,075 bytes and Dropbox unknown-status snapshots 3,518,307 bytes. Those totals are not estimates of safely reclaimable space. All files were retained.

The local audit helper is `work/audit-cloud-retention.py`. Aggregate evidence is in `work/cloud-retention-20261017-google.json` and `work/cloud-retention-20261017-dropbox.json`; these are untracked local files. That initial design pass involved source review and a read-only provider audit, with no application change or new Windows binary. The subsequent [native storage review validation](NEO_CLOUD_STORAGE_REVIEW.md#windows-validation-2026-10-17) used a rebuilt Windows package and confirmed the same inventories. Neither pass enables cleanup.

## Implementation and validation sequence

1. **Complete at `ebaf24069`:** native read-only storage review and a pure classification component. Regression coverage includes complete/paginated listings, shared references, malformed/unknown manifests, duplicate names/IDs, account change, cancellation and preserving original data. Both providers passed review in Windows Clean and Modern against disposable accounts, plus cancellation/retry. See [implementation and evidence](NEO_CLOUD_STORAGE_REVIEW.md).
2. **Development increment implemented:** the [experimental history workflow](NEO_CLOUD_HISTORY.md) adds a gated schema 3 namespace, schema 1/2 copy migration, explicit history saves/restores, registered upload tickets and restore reservations. Existing sync remains unchanged. Pristine-root convergence, explicit legacy-branch copying, local authority checkpoints and read-only active-branch review have passed Windows validation. The accepted [explicit workspace model](NEO_CLOUD_WORKSPACES.md) adds self-bound schema 4 controls, exact-ID verification, immutable-reference checkpoints and guarded local choice; 600 native cases and 20 native live Windows stages passed. Guarded creation/join/copy migration and coordinated automatic-sync integration remain required before production use. Do not add deletion yet.
3. **Disposable provider probes completed 2026-10-27:** conditional deletion, stale revisions, competing callers and committed-but-unread acknowledgements were tested on eleven newly created probe objects; see [measured results and limitations](NEO_CLOUD_DELETE_PROBES.md). Native adapters, retirement preview, durable state and bounded deletion batches remain to implement. Enable opt-in cleanup only after the matrix below passes on the exact Windows build. Physical Android remains a separate gate; do not imply it has been covered by Windows results.

| Required adversarial test | Required outcome |
| --- | --- |
| Writer suspended after upload while collector retires the attempt | Old publish rejected; retry uses a new identity; current log remains readable |
| Writer publishes before retirement CAS | Stale collector plan rejected; committed object remains protected |
| Reader/restore paused across retirement planning | Reservation prevents deletion; local edits survive any retry |
| New reader or pin races the `retired` to `deleting` transition | Exactly one transition wins; no read is admitted after the deletion fence |
| Commit/delete response lost; app stopped at each transition | Restart determines actual state without deleting the head or reviving a retired ticket |
| Two collectors and a writer race | Only valid conditional transitions succeed; repeated exact-object deletion is harmless |
| Google duplicate roots, delayed creates or ambiguous payload names | Cleanup blocked until the explicitly selected physical root/epoch and each payload's ownership are verified; listing never elects authority |
| Old client writes legacy namespace during/after migration | New namespace remains intact; separate legacy changes are surfaced and preserved |
| Missing page, stale cursor, changed account, malformed metadata, unexpected object revision | No deletion from the incomplete or invalid plan |
| Recovery branch, legacy copy, pin, unknown file or long-offline device | Protected data retained; device re-establishes current state without silent local replacement |
| Age/count boundary, clock skew or lost time evidence | Latest 20 and entire 30-day window retained; uncertainty extends retention |
| Quota exhaustion or safety-metadata limit | Clear error and intact local/remote log; no emergency sweep |

Retention design and native read-only storage review are complete. Experimental coordinated history and restore are available behind the development gate. The owner chose [explicit workspaces](NEO_CLOUD_AUTHORITY_CHOICES.md) on 2026-10-28 JST; provider/account/epoch/physical-root binding replaces automatic per-account root election. Its unused guarded empty-root creator now passed 732 native cases, exact Windows validation and native live interruption/concurrent-process checks. Shared application create/join/migration, automatic-sync integration, native conditional deletion adapters and cleanup validation are still required; this does not close Milestone 13 or authorize deleting existing recovery data.

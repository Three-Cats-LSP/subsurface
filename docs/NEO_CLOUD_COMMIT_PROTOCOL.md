# Neo cloud commit protocol

Implemented and live-validated on Windows for both providers at `ed8a15fd4` (2026-10-16). Milestone 13 remains open for the remaining release gates.

Google Drive and Dropbox now publish schema 2 manifests. XML remains the canonical dive-log format. Each attempt creates `subsurface-neo-<revision UUID>.xml`; the fixed `subsurface-neo-sync.json` manifest names that immutable payload and its SHA-256. The previous XML remains readable until the manifest commit succeeds. Readers follow the manifest's exact payload name and validate the checksum and complete canonical XML before replacement.

## Commit and recovery

1. Read the manifest together with its provider revision. Explicit backup and Keep this device also read this revision before uploading.
2. Create a unique XML snapshot without overwriting an existing file.
3. Confirm the local log still matches the captured snapshot.
4. Update the manifest only if its provider revision still matches. A stale writer preserves its local log and reports a conflict; it never automatically retries the overwrite.
5. Re-read the manifest by name before recording success or advancing the local baseline.

Dropbox uses `mode=update` with the downloaded `rev`, or `mode=add` for creation, with `strict_conflict=true` and `autorename=false`. Google uses the v2 File `etag` and a v2 media PUT with `If-Match`. Live disposable-file probes verified that stale writes return HTTP 409 and 412 respectively. A v3 media PATCH ignored `If-Match` in the probe and must not be substituted. Google metadata is read before and after the media body; differing ETags abort the read. Metadata and media ETags are not interchangeable.

An upload interrupted before manifest publication leaves the previous committed log intact. Retry starts with a fresh manifest read. If the server committed but the success response was lost, an identical manifest payload is recognized as up to date without another write. Partial or unreferenced uploads are not used by readers. Old and abandoned snapshots are retained for recovery; automatic garbage collection is intentionally absent until a safe retention protocol exists.

## Migration and limits

Schema 1 remains readable and continues to reference `subsurface-neo.xml`. The first changed upload publishes schema 2 without changing that legacy XML. Upgrade every syncing client before using schema 2: older clients reject the new schema, and older explicit backup implementations can still overwrite a manifest without conditional protection.

Drive allows duplicate filenames and cannot conditionally create a filename. First-time setup now recovers duplicate initial manifests during lookup. Clients sort by provider creation time, then file ID for ties, retaining the earliest file as the active manifest. Every candidate is read with a stable revision before recovery. Only schema 2 candidates with no parent can be archived; invalid manifests or an established competing branch stop recovery without discarding either log.

Each competing initial manifest is renamed to `subsurface-neo-sync-recovered-<Drive file ID>.json` with a v2 metadata PATCH and `If-Match`. Its contents and immutable XML are retained. The rename changes its ETag, fencing writers that captured the old revision. A stale rename triggers a fresh lookup; a final lookup also catches a delayed initial create. Recovery is bounded to eight attempts, and interrupted recovery resumes on the next sync. Reads verify the file title as well as its revision, so a client cannot mistake an archived candidate for the active head. No recovery file is automatically deleted.

Identical initial logs can finish against the surviving manifest and save its actual revision as their baseline. Different logs retain the local copy and use the existing conflict/initial-choice flow. Recovery selects a sync head; it does not merge dive logs. Dropbox continues to atomically reject a conflicting first creation.

The selection assumes the provider returns the current matching files and preserves their server creation times. It does not use device clocks or modification times. Manually changing creation metadata, externally deleting the active manifest, legacy clients that bypass conditional writes, and established divergent branches are outside automatic first-time recovery.

These changes do not include equipment-library, manual-collection, or planner-preset synchronization. Public OAuth approval and physical Android validation remain separate work.

## Validation

The native regression suite models both providers and covers concurrent writers, initial creation races, rejected payloads, failed manifest commits, lost commit responses, changes during upload, missing revisions, and a cloud update during a Google manifest read. Live Windows checks passed migration, a controlled two-writer race, a connection cut during a large upload, process termination before manifest publication, restart/retry, and process termination after remote commit but before local confirmation. Separate readers verified the old log during interrupted attempts and the new log after recovery. Both disposable cloud logs were restored to one dive. See [Google evidence](NEO_GOOGLE_DESKTOP_TESTING.md) and [Dropbox evidence](NEO_DROPBOX_DESKTOP_TESTING.md).

[Windows build, launch, Clean/Modern and lifecycle checks](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38909378239), [36 desktop and 33 mobile-target regression tests](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38909378266), and [planner regression](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38909378290) passed at `ed8a15fd4`.

Provider references: [Google v2 File resource](https://developers.google.com/workspace/drive/api/reference/rest/v2/files), [Google v2 update](https://developers.google.com/workspace/drive/api/reference/rest/v2/files/update), [Dropbox SDK file-write contract](https://dropbox-sdk-python.readthedocs.io/en/latest/api/files.html).

The initial-recovery regression cases additionally cover two recovering clients, identical and different logs, delayed creation, stable ordering and timestamp ties, stale rename rejection, lost acknowledgements, partial recovery/restart, cancellation, invalid candidates, and bounded contention. A live disposable-file probe verified Google v2 metadata PATCH: first rename HTTP 200, stale rename HTTP 412, original contents retained. Windows end-to-end validation of this follow-up is pending.

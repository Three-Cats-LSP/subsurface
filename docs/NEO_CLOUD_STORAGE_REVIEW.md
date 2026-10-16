# Read-only cloud storage review

Cloud & Sync now offers **Review storage** for connected Google Drive and Dropbox accounts. The shared page is used by Clean and Modern. A completed review shows its observation time, total items/bytes and five mutually exclusive categories:

| Category | Contents |
| --- | --- |
| Current log files | Active manifest and the payload it references |
| Recovery files | Recognized recovered manifests and their referenced payloads, counted once per object |
| Legacy log | Legacy XML not already counted as current/recovery |
| Other snapshots | UUID snapshots whose commit history cannot be established |
| Other items | Remaining files and folders |

Other snapshots are not labelled disposable or reclaimable. The feature has no deletion, upload, rename, restore or migration action. See the [retention design](NEO_CLOUD_RETENTION.md) for the work required before cleanup can be enabled.

## Native behavior

`CloudSyncManager::reviewStorage()` verifies the connected account afresh, renewing authorization through the existing token path when needed. It shares the normal cloud-operation busy state, so automatic sync and other UI cloud operations cannot overlap the review. **Cancel** stops pending identity, refresh or review requests and suppresses late results. Disconnect and account changes clear stored review results. A new review removes its previous result before starting; failures never leave stale totals presented as the new review.

`CloudStorageReview` has a separate read-only transport and a pure classification function. It does not call the sync file lookup, which can rename competing initial Google manifests. Google requests use App Data; Dropbox lists the app folder's top level and explicitly discloses unscanned folder contents. Every pagination token/cursor is followed within the review bounds. The transport reads recognized manifest bodies by exact object ID, resolves their payload references from metadata, then repeats the complete inventory. A revision, name, size or identity change invalidates the review. Two matching inventories are an observation, not a cross-file transaction or a future deletion authorization.

The review does not download XML bodies or unrelated file contents, change the local dive model, establish a sync baseline, assign a provider or record a successful sync. Counts and sizes describe stored objects; they do not validate each dive log's contents. Results stay in memory and are labelled with the review time. Restart requires a fresh review, and an upload invalidates its provider's prior result.

An empty cloud folder can return zero items. Missing/ambiguous active manifests in a nonempty inventory, invalid or unsupported manifests, missing/ambiguous reference targets, repeated IDs/cursors, incomplete pagination/search, invalid sizes/revisions, overflow, network errors and account changes leave the review incomplete. Unknown files are kept. Provider error bodies and file/account identifiers are not included in the review's error messages or result model.

## Bounds

- Each listing pass: at most 100 pages and 20,000 entries.
- Each listing response: at most 2 MiB; each recognized manifest: at most 256 KiB.
- At most 128 recognized manifests per review, with sequential reads.
- File requests: 30-second inactivity timeout and 35-second request deadline; the complete file review has a 180-second deadline after authorization/identity checking.
- Folder sizes are not treated as recursive byte totals. Byte sums use checked 64-bit arithmetic.

Exceeding a bound reports an incomplete review and leaves retry available. No partial count is published as a complete result.

## Regression coverage

`TestCloudSyncReview` covers classification of shared recovery references and schema 1/2 manifests, negative/overflowing sizes, ambiguous references, malformed manifests, both providers' paginated transport, missing metadata, incomplete/repeated pagination, concurrent inventory changes, request failures, unauthorized tokens, folder/empty inventories, cancellation and successful retry. It asserts that only listing and manifest-download requests occur, never XML downloads or provider mutations.

`TestCloudSyncManager` adds review coverage for fresh identity checking, suppression of automatic sync, unchanged local XML and baseline, account mismatch/failure, cancellation during identity or inventory, disconnect, retry and HTTP 401 recovery behavior. Tests use isolated settings and synthetic credentials.

## Windows validation, 2026-10-17

Application revision `ebaf24069399ea85819fa8d36a6f9826b7108058` passed the following exact-head checks:

- [Windows build, packaged launch, Clean/Modern workflows and automatic sync lifecycle](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38940833740).
- [All 37 desktop and 34 mobile-target cloud regression test executables](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38940833720), including 63 new parameterized cases across the review and manager suites in each target.
- [Planner regression](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38940833681), [QML checks](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38940833710) and [Android compilation](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38940833694).

The Windows package then passed six native stages against the existing disposable Google Drive and Dropbox logs: a Clean review, a Modern review and a Modern cancellation/retry for each provider. An instrumented QML driver invoked the actual page buttons in isolated Windows profiles. It stalled provider traffic through a profile-only CONNECT proxy after fresh identity verification, cancelled the review, observed no late result, then restored traffic and completed a new review. This tests application cancellation and retry; it is not a physical network-adapter outage test.

| Live review category | Google items / bytes | Dropbox items / bytes |
| --- | ---: | ---: |
| Current log files | 2 / 871 | 2 / 871 |
| Recovery files | 29 / 9,518 | 0 / 0 |
| Legacy log | 1 / 537 | 1 / 537 |
| Other snapshots | 10 / 3,521,075 | 6 / 3,518,307 |
| Other items | 5 / 1,350 | 0 / 0 |
| Total | **47 / 3,533,351** | **9 / 3,519,715** |

All six runs matched the earlier read-only audit. Canonical local XML remained byte-identical before/after each review and across restarts. Sync baselines and empty provider assignments were preserved; results did not persist across restart. Original test-profile credentials were unchanged. Independent final inventories matched the earlier inventories exactly by metadata digest, and separate reads confirmed the original current manifests and payloads were unchanged. The review did not validate the contents of every recovery or unknown snapshot.

Twelve captures recorded the result section at 1,100- and 390-pixel application widths; inspected wide/narrow captures showed wrapping without clipped result text. These are result-section captures, not a full-page manual accessibility audit. The first driver attempt returned correct totals but captured before the Modern navigation had settled. The driver was corrected to wait for the shared page and layout, and all stages were rerun successfully without changing application code. Initial failed-attempt logs are retained separately.

An independent cleanup audit confirmed all ten processes from the initial and successful attempts had exited, all eight temporary credential keys were absent and all sixteen temporary proxy values were absent. No original profile or unrelated local file was removed. Local evidence is retained under `work/native-storage-review-{google,dropbox}-ebaf24-retry/`, with aggregate verification in `work/native-storage-review-audit-ebaf24.json` and `work/cloud-retention-20261017-{google,dropbox}-post-review-ebaf24.json`; these files are untracked.

Physical Android runtime and the unavailable second Dropbox account remain outside this Windows test pass. Coordinated history/restore, migration and deletion validation remain pending; storage review does not close Milestone 13 or enable cleanup.

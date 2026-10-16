# Experimental coordinated cloud history

The next retention increment adds a gated history namespace with copy migration, explicit saves and restore. Normal schema 1/2 synchronization stays in place. This is a development workflow for isolated profiles; it is not the production migration of automatic sync. Deletion, retirement, reservation expiry and metadata compaction are absent.

## Development gate and workflow

Set `SUBSURFACE_NEO_HISTORY_DEV_EPOCH` to a newly generated lowercase UUID before launching the test application. The shared Clean/Modern Cloud & Sync page then exposes **Experimental history**. Without a valid epoch the controls are hidden and the manager entry point refuses history operations. The epoch selects the same namespace across restarts or collaborating test clients; never reuse it for a different history. Keep the epoch in local test evidence, not in diagnostics containing credentials.

Use an isolated `--user` profile, turn automatic sync off and leave both provider assignments empty. The manager enforces those conditions, verifies the provider account afresh and uses the existing credential refresh path. Ordinary sync, storage review and history share the busy state. Disconnect and cancellation stop pending history requests. Account changes clear displayed reports and the existing baseline/assignment protections still apply.

1. **Load history** reads the selected namespace. A missing namespace is reported as uninitialized; this does not create a file.
2. **Initialize history** reads a valid schema 1/2 manifest and checksum-valid, complete canonical XML, then copies that log into the new namespace. It preserves every original object. An interrupted initialization can be retried; existing tickets and objects remain recorded or retained, and the new attempt gets a fresh identity.
3. **Save version** explicitly publishes the current local canonical XML to history. It retains all earlier versions and does not update the old sync manifest, baseline or last-success time.
4. **Restore** asks for confirmation, requires the current local bytes to match the history head, reserves the chosen version and validates its exact object/revision, length, checksum and XML. It publishes the chosen contents as a new snapshot, retaining the former head, before applying and persisting the replacement locally. Unsaved local changes must first be saved as a history version. Concurrent local edits prevent replacement. Reload after an uncertain outcome; a cloud commit may have succeeded even when local confirmation failed.

History results are held in memory and must be loaded again after restart. Displayed dates are client observation/creation times, not authoritative commit times for a retention policy. Only canonical XML is included; separate Neo equipment, collections and planner settings retain their existing scope.

## Control record and concurrency

The experimental record uses `schema: 3` and `format: neo-history-development-1` at `subsurface-neo-history-<epoch>.json`. It is bound to the provider, a hash of the verified stable account ID and the namespace epoch. Payload names contain both the epoch and a never-reused upload UUID. The format remains provisional and unsupported by schema 1/2 clients.

The single record contains ordered committed history, the current head, durable upload tickets and read/restore reservations. Each mutation uses the provider's already validated conditional-write operation and is followed by a versioned reread. A conflicting write stops; it never overwrites a newer record or retries against a newer head automatically. A lost acknowledgement triggers one read to confirm the intended record. If another mutation prevents exact confirmation, the operation stops with all data retained and asks for a reload.

- A ticket is registered through the root before any payload upload. The unique payload is created without overwrite, read back, and its provider object ID/revision, length and hash are recorded before publication.
- Publication requires an uploaded ticket whose expected parent is still the current head. The same conditional root write appends committed history and advances the head. Pending or previously consumed ticket identities cannot be reused.
- Restore reserves the source through the same root before its exact-object read. The reservation and head are checked again before registering the replacement upload. Restore provenance must match the source length/hash. A completed restore releases its reservation through another conditional root write.
- Cancellation or process failure may leave tickets, uploaded objects or active reservations. They remain protected. There is no age-based inference of abandonment and no automatic expiry.
- Unknown record fields, unsupported versions/states, invalid identities, references or sizes, inconsistent history and safety limits stop mutation. The implementation does not rewrite away unknown safety metadata.

Google permits duplicate names. Strict history reads reject duplicate roots and duplicate legacy manifests; they never invoke the existing initial-manifest rename recovery. A simultaneous first initialization can therefore require a fresh test namespace. This safely blocked state is not a completed root-convergence protocol and must be resolved before any future cleanup rollout. Exact source reads verify the recorded object and revision rather than choosing another object with the same name.

The source schema 1/2 manifest fingerprint is retained in the root. Every operation compares it, and publication checks it again. A later legacy write is surfaced as a separate branch: reading history remains possible, while further history writes are paused. Neither branch is merged or erased automatically. Resolving those branches, durable authority selection after external root removal/replacement, coordinated automatic sync and rollback UX remain future migration work.

## Bounds and remaining retention gates

The development record is limited to 1 MiB, 512 tickets and 512 reservation records. Payloads/responses are limited to 64 MiB; the file phase has a 180-second deadline after authorization/identity checking, in addition to the existing request inactivity timeout. Reaching a bound stops new work without removing older versions or safety metadata. Released reservation records and consumed tickets are retained.

All versions and prior schema 1/2 files remain protected. The [retention design](NEO_CLOUD_RETENTION.md) still requires authoritative root convergence, provider-backed time evidence, pins/recovery roots, retirement/deletion fences, safe migration completion and provider deletion validation. This development increment must not be treated as permission to add a sweep to the existing schema 2 namespace.

## Validation

The native regression suite exercises schema 1/2 copy migration, save/restore with new identities, restart, active reservations, ticket reuse and stale-parent rejection, malformed/unsupported records, simultaneous initialization, failed uploads, lost root acknowledgements, cancellation, changed legacy branches, corrupted/revised/renamed source objects, account/epoch mismatch and local edits during restore. Manager cases cover the disabled gate, assignment/automatic-sync restrictions, fresh identity, cancellation, disconnect and identity failure. Provider simulations assert conditional root writes and ticket registration before payload creation, and prohibit legacy writes or deletion requests.

### Exact-head checks

Application revision `9c3d3dcf0409b635d778a2b2440c6bc9eab00110` passed:

- [Windows build, packaged launch, Clean/Modern workflows and automatic-sync lifecycle](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38946433923).
- [All 38 desktop and 35 mobile-target regression executables](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38946434140), including 49 history cases and seven new manager guard cases in each target.
- [Planner regression](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38946434034), [QML validation](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38946434007) and [Android compilation](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38946433944).

### Live Windows, 2026-10-17

The packaged application passed the following sequence on both existing disposable provider accounts. Native QML instrumentation invoked the page buttons and restore confirmation in isolated Windows profiles; provider data requests used the actual APIs through a profile-only CONNECT proxy. A fresh epoch was used for each test namespace.

| Stage, per provider | Result |
| --- | --- |
| Clean initialization from the schema 2 cloud log | One copied history version; original cloud files and local XML preserved |
| Modern save after importing a second synthetic dive | Second history version; local two-dive log retained after restart |
| Modern restore after restart changed calculated metadata | Refused before any history mutation or local replacement |
| Modern save of the current, recalculated log | Third history version; earlier versions retained |
| Modern confirmed restore of the initial version | Fourth version with a fresh snapshot identity; local log returned to the original one dive |
| Clean restart and history reload | Four versions visible; restored local XML byte-identical to the starting canonical export |

The rejected restore exposed a useful strictness limit: the synthetic manual dive initially lacked mean depth. Subsurface generated its profile, then added calculated mean depth on the next reload, changing the serialized XML. The history guard correctly required a new explicit save. The test verified that refusal left the two-version root, tickets and local XML intact, then exercised successful save/restore recovery. The guard deliberately compares bytes and does not infer that a metadata difference is harmless.

Ten completed native stages and two safe restore refusals were verified. Twenty wide/narrow result-section captures were recorded at 1,100- and 390-pixel application widths; inspected captures showed readable wrapping and controls. These are instrumented native workflow checks, not a full manual accessibility audit. Schema 1 migration, simultaneous initialization and injected provider-failure cases were exercised in regression simulations, not claimed as new live-provider tests.

Before the completed sequence, a driver assertion compared differently formatted original and cloned baseline JSON bytes. Both initializations had succeeded, but that harness check failed. The driver was corrected to compare the clone against its own starting bytes. Those two original attempt logs and their synthetic namespaces were retained; no application fix was required.

Final independent read-only audits excluded only the four exact test epochs and reproduced the prior inventory digests for all **47 pre-existing Google objects and nine pre-existing Dropbox objects**. Original current manifests/payloads were also read and compared. Original profile credentials and legacy baselines were preserved. The completed namespaces each retain a root and four snapshots; the initial harness attempts each retain a root and one snapshot, for **14 newly created synthetic objects kept in total**. No deletion request was made.

Cleanup verification found all fourteen native test processes exited, all eight temporary credential keys absent and all sixteen temporary proxy values absent. Local test evidence remains under `work/native-history-{google,dropbox}-9c3d3-retry/`, with initial attempts under `work/native-history-{google,dropbox}-9c3d3/`. Aggregate audits are `work/native-history-audit-9c3d3-retry.json` and `work/cloud-retention-20261017-{google,dropbox}-post-history-9c3d3.json`. These are untracked local files.

This closes the gated manual history/restore increment. It does not close Milestone 13, production migration or cleanup. Physical Android and two-live-account Dropbox tests remain deferred.

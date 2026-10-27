# Explicit cloud workspaces

The owner chose explicit workspace selection on 2026-10-28 JST, keeping Google App Data/Dropbox App Folder permissions and avoiding a coordination service. Concurrent creators may create separate protected workspaces. A new device explicitly joins one workspace; discovery does not silently choose or merge roots.

## Reference and local choice

`CloudWorkspaceReference` identifies one provider, account hash, fresh epoch and physical root ID. Its canonical JSON SHA-256 is the workspace key. Google roots with equal names and epochs but different physical IDs have different keys. Payload names include that complete key and a fresh ticket UUID, so even these roots cannot claim each other's new snapshots by name. References contain neither credentials nor XML and are not authorization.

`CloudWorkspaceSelection` records local user choice in a separate file, scoped to a provider/account. Selection reloads under `QLockFile`, checks the expected choice **version**, writes atomically with `QSaveFile` and reads back the result. Each changed choice gets a fresh UUID version independent of the selected workspace key: switching A → B → A cannot let a request holding the first A version select C afterward. An already completed choice reconciles without rewriting. Switching retains every previously known reference; a limit of 32 blocks a further new choice without evicting anything. Corrupt, unknown, oversized, foreign-scope or inconsistent references block overwrite. This file never replaces an existing history checkpoint, schema 2 baseline, credentials or local XML.

The selection file is now `neo-workspace-selection-development-2`; earlier version-1 files lack the independent choice version and are preserved/refused rather than silently upgraded. The first implementation has no application UI caller, and live tests did not create selection files. Eight new native rows cover the away/back race, old files and invalid expected/stored versions on both providers.

Capture the expected choice version **before** starting asynchronous verification. Re-reading the current choice version just before committing would let an older pending user action override a more recent choice.

Native regression cases cover equal-name roots, distinct payload ownership, invalid/foreign/unknown references, restart, concurrent stale choice, lost local acknowledgement, locks, blocked paths, corruption, size bounds and preservation of earlier choices/checkpoints. This increment does not use live cloud credentials or publish cloud objects.

## Self-bound records and exact verification

The next increment adds `CloudWorkspaceRecord`, a separate development schema 4 envelope containing the complete reference and coordinated journal. Every wire ticket name must equal the reference's root-qualified payload name. Provider/account/epoch must match the journal; completed payloads need valid provider IDs, and the root itself cannot be a payload. Unsupported/unknown fields, identity changes, missing pending tickets, rollback and inconsistent restore reservations are refused. There is no deletion, retirement or expiry transition.

Journal operations reuse `CloudHistoryRecord`'s validated state machine by normalizing ticket names **only in memory**, after checking every wire name against its workspace. Replacing the journal restores workspace-qualified names before validation/publication. Old history readers reject the schema 4 envelope. This does not convert existing history or make its old snapshots collectible.

`CloudWorkspaceOpen` reads only the supplied exact root ID and expected workspace name. Google metadata brackets its body read; Dropbox verifies download metadata. It checks the self-reference, current scope guard and optional prior checkpoint extension, then returns the confirmed control record/revision. It performs no filename lookup, election, cloud mutation, local choice/checkpoint update or XML application. Its caller must verify account identity before calling and check scope again before persisting a choice. Cancellation, unknown/changed/missing roots, account changes, invalid metadata and oversized records release the operation with no binding change.

Ninety-four additional native rows passed on Windows/Linux at `a66d0ad11`, for 558 cases total, covering record transitions and both-provider verification, including distinct equal-name roots, wrong self-binding/epoch/provider/account, pending ownership, invalid object identity, rollback, missing/future controls, stale checkpoints, scope changes, cancellation and metadata faults. The developer utility adds a read-only `workspace-open` mode restricted to marked synthetic fixtures; token input remains anonymous stdin. Ten matching native live stages passed on each provider. Compilation/live details and package status are recorded in the continuation checkpoint.

## Durable per-workspace observations

`CloudWorkspaceAuthority` adds a separate metadata-only checkpoint for one immutable reference. It reloads the latest file under its process lock, accepts only a monotonic extension of the complete prior journal, atomically saves the control record/provider revision and verifies the readback. An identical observation reconciles without a rewrite; an unchanged body with a new provider revision can advance that revision. Switching the user choice or applying XML is outside this component.

Foreign account/root references, rollback, lost pending tickets/reservations, unknown or corrupt checkpoint files, unsafe revisions, held locks and blocked/oversized paths refuse replacement. A failed load clears cached observations. Missing-file load returns an empty observation without creating directories; callers must distinguish an explicitly reviewed first join from a selected workspace whose checkpoint has disappeared. The latter requires recovery, not silent fresh authority. Thirty-four additional native filesystem rows passed on Windows/Linux at `8e25354c9`, for 592 cases total, including stale processes, restart, idempotence and unchanged selection/schema 2 baseline/XML files.

## Guarded creation and binding plan

This sequence is still to implement; the native live reader fixtures use a test harness for creation and are not evidence of a production create workflow.

1. Explicit Create allocates a fresh epoch/operation nonce and durably records a local creation intent before contacting the provider. An unbound bootstrap object admits no upload tickets and no writer/reader authority. Never create over an established workspace ID or resolve an uncertain established root through a name.
2. Create only a new object. Return its physical ID and revision directly from the provider acknowledgement, and durably save them before binding it. If the creation acknowledgement is lost, retain the incomplete object and report uncertainty; do not replay a name-based replacement. A later explicit Create allocates another fresh identity. A listing can offer objects for review, not establish that an uncertain create never committed.
3. Bind the self-reference to that exact physical ID with its observed revision CAS. Confirm by exact-ID read and persist a per-workspace monotonic checkpoint. If the bind acknowledgement is lost, re-read that same ID and compare the complete control state. Never fall back to another same-name root.
4. Copy migration verifies the complete source manifest/payload, checksum and canonical XML, registers fresh root-qualified tickets before uploading, and verifies each new exact object/revision before publication. Preserve every original object and source fingerprint. A legacy write racing migration is a protected branch, not permission to overwrite either history.
5. Save a durable checkpoint before committing the expected local workspace choice. A failed/stale choice leaves the previous selection active and the newly observed workspace checkpoint retained for recovery. Explicit selection and remote-only migration do not apply XML locally; local replacement requires a separate confirmation and unchanged-local-data check.

Each later operation must reload its selected reference and per-workspace checkpoint, verify fresh provider/account scope and read the exact root before CAS or reservation. Validate all payload identities, names, lengths, checksums and XML separately; a valid root control record does not prove its payload contents. Unknown, missing, corrupt or conflicting local checkpoints block activation rather than resetting authority. First-device verification with no prior checkpoint and later anti-rollback observation are distinct cases.

## Remaining activation gates

Before using a reference, verify current provider account identity, read that exact physical root with a stable revision and validate its immutable self-reference, epoch and supported control format. A reference alone is insufficient to select writable authority. Re-check scope before persisting a choice and before each subsequent operation. Missing or uncertain established roots must never be recreated by filename.

Creation must bind a new physical root before registering payload ownership. A crash before that binding leaves a protected incomplete root, not authority. Copy migration must retain its complete original source and allocate root-qualified fresh payload identities; old schema 1/2 and development history objects stay permanently protected. Join/selection preserves local data. Replacing or merging local dive contents remains a separate confirmed operation with a local-change guard.

References, self-bound records, exact-root verification, durable checkpoints and versioned local choices are validated: 600 native cases passed on Windows/Linux, 20 matching native live Windows stages passed across both providers, and exact combined 3b Windows/Clean/Modern/lifecycle package run `40548628043` passed. See the continuation checkpoint for exact sources, artifacts and limits; do not repeat these completed checks without a new reason.

Next implement the guarded creation protocol above, then expose create/join through the shared Clean/Modern page. Validate its native races, interruption/restart and live disposable provider namespaces before enabling automatic use. No application UI invokes these new components yet. No deletion operation or production cleanup is enabled by these foundations.

# Explicit cloud workspaces

The owner chose explicit workspace selection on 2026-10-28 JST, keeping Google App Data/Dropbox App Folder permissions and avoiding a coordination service. Concurrent creators may create separate protected workspaces. A new device explicitly joins one workspace; discovery does not silently choose or merge roots.

## Reference and local choice

`CloudWorkspaceReference` identifies one provider, account hash, fresh epoch and physical root ID. Its canonical JSON SHA-256 is the workspace key. Google roots with equal names and epochs but different physical IDs have different keys. Payload names include that complete key and a fresh ticket UUID, so even these roots cannot claim each other's new snapshots by name. References contain neither credentials nor XML and are not authorization.

`CloudWorkspaceSelection` records local user choice in a separate file, scoped to a provider/account. Selection reloads under `QLockFile`, checks the expected previous choice, writes atomically with `QSaveFile` and reads back the result. An already completed selection reconciles without rewriting. Switching retains every previously known reference; a limit of 32 blocks a further new choice without evicting anything. Corrupt, unknown, oversized, foreign-scope or inconsistent references block overwrite. This file never replaces an existing history checkpoint, schema 2 baseline, credentials or local XML.

Native regression cases cover equal-name roots, distinct payload ownership, invalid/foreign/unknown references, restart, concurrent stale choice, lost local acknowledgement, locks, blocked paths, corruption, size bounds and preservation of earlier choices/checkpoints. This increment does not use live cloud credentials or publish cloud objects.

## Self-bound records and exact verification

The next increment adds `CloudWorkspaceRecord`, a separate development schema 4 envelope containing the complete reference and coordinated journal. Every wire ticket name must equal the reference's root-qualified payload name. Provider/account/epoch must match the journal; completed payloads need valid provider IDs, and the root itself cannot be a payload. Unsupported/unknown fields, identity changes, missing pending tickets, rollback and inconsistent restore reservations are refused. There is no deletion, retirement or expiry transition.

Journal operations reuse `CloudHistoryRecord`'s validated state machine by normalizing ticket names **only in memory**, after checking every wire name against its workspace. Replacing the journal restores workspace-qualified names before validation/publication. Old history readers reject the schema 4 envelope. This does not convert existing history or make its old snapshots collectible.

`CloudWorkspaceOpen` reads only the supplied exact root ID and expected workspace name. Google metadata brackets its body read; Dropbox verifies download metadata. It checks the self-reference, current scope guard and optional prior checkpoint extension, then returns the confirmed control record/revision. It performs no filename lookup, election, cloud mutation, local choice/checkpoint update or XML application. Its caller must verify account identity before calling and check scope again before persisting a choice. Cancellation, unknown/changed/missing roots, account changes, invalid metadata and oversized records release the operation with no binding change.

Ninety-four additional native rows are prepared for record transitions and both-provider verification, including distinct equal-name roots, wrong self-binding/epoch/provider/account, pending ownership, invalid object identity, rollback, missing/future controls, stale checkpoints, scope changes, cancellation and metadata faults. The developer utility adds a read-only `workspace-open` mode restricted to marked synthetic fixtures; token input remains anonymous stdin. Compilation/live validation results are recorded separately in the continuation checkpoint.

## Remaining activation gates

Before using a reference, verify current provider account identity, read that exact physical root with a stable revision and validate its immutable self-reference, epoch and supported control format. A reference alone is insufficient to select writable authority. Re-check scope before persisting a choice and before each subsequent operation. Missing or uncertain established roots must never be recreated by filename.

Creation must bind a new physical root before registering payload ownership. A crash before that binding leaves a protected incomplete root, not authority. Copy migration must retain its complete original source and allocate root-qualified fresh payload identities; old schema 1/2 and development history objects stay permanently protected. Join/selection preserves local data. Replacing or merging local dive contents remains a separate confirmed operation with a local-change guard.

Next validate exact-root verification and self-bound records, add durable per-workspace checkpoints and a guarded creation protocol, then expose create/join through the shared Clean/Modern page. Validate native races, interruption/restart and live disposable provider namespaces before enabling automatic use. No application UI invokes these new components yet. No deletion operation or production cleanup is enabled by these foundations.

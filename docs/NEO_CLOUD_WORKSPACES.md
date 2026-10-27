# Explicit cloud workspaces

The owner chose explicit workspace selection on 2026-10-28 JST, keeping Google App Data/Dropbox App Folder permissions and avoiding a coordination service. Concurrent creators may create separate protected workspaces. A new device explicitly joins one workspace; discovery does not silently choose or merge roots.

## Reference and local choice

`CloudWorkspaceReference` identifies one provider, account hash, fresh epoch and physical root ID. Its canonical JSON SHA-256 is the workspace key. Google roots with equal names and epochs but different physical IDs have different keys. Payload names include that complete key and a fresh ticket UUID, so even these roots cannot claim each other's new snapshots by name. References contain neither credentials nor XML and are not authorization.

`CloudWorkspaceSelection` records local user choice in a separate file, scoped to a provider/account. Selection reloads under `QLockFile`, checks the expected previous choice, writes atomically with `QSaveFile` and reads back the result. An already completed selection reconciles without rewriting. Switching retains every previously known reference; a limit of 32 blocks a further new choice without evicting anything. Corrupt, unknown, oversized, foreign-scope or inconsistent references block overwrite. This file never replaces an existing history checkpoint, schema 2 baseline, credentials or local XML.

Native regression cases cover equal-name roots, distinct payload ownership, invalid/foreign/unknown references, restart, concurrent stale choice, lost local acknowledgement, locks, blocked paths, corruption, size bounds and preservation of earlier choices/checkpoints. This increment does not use live cloud credentials or publish cloud objects.

## Remaining activation gates

Before using a reference, verify current provider account identity, read that exact physical root with a stable revision and validate its immutable self-reference, epoch and supported control format. A reference alone is insufficient to select writable authority. Re-check scope before persisting a choice and before each subsequent operation. Missing or uncertain established roots must never be recreated by filename.

Creation must bind a new physical root before registering payload ownership. A crash before that binding leaves a protected incomplete root, not authority. Copy migration must retain its complete original source and allocate root-qualified fresh payload identities; old schema 1/2 and development history objects stay permanently protected. Join/selection preserves local data. Replacing or merging local dive contents remains a separate confirmed operation with a local-change guard.

Next implement exact-root verification and self-bound new history records, then expose guarded create/join through the shared Clean/Modern page. Validate native races, interruption/restart and live disposable provider namespaces before enabling automatic use. No deletion operation or production cleanup is enabled by these foundations.

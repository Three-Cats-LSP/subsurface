# Local history authority checkpoints

Implemented 2026-10-27; validation of the new revision is pending. This is another development-gated increment of [coordinated cloud history](NEO_CLOUD_HISTORY.md). It does not migrate ordinary synchronization or enable cleanup.

## Stored state and boundaries

Before using a history root, native Neo records its exact provider object identity and complete validated control record in local application data under `neo-history-authority/`. The filename hashes the settings/profile scope, provider, verified account hash and namespace epoch. The checkpoint contains version/ticket/reservation metadata, never XML payloads, account email addresses or OAuth credentials. OAuth tokens retain their existing secure-store path.

Writes use `QSaveFile` atomic replacement and a per-checkpoint `QLockFile`. A competing process, corrupt/unsupported checkpoint or storage error stops the operation without replacing the checkpoint. Each observation reloads the checkpoint under the lock, including before root writes. A missing or replaced established root is not silently recreated or adopted. Disconnect and grant rejection preserve these recovery checkpoints; account and namespace scopes remain distinct.

A later root must preserve the observed history prefix, every upload ticket, immutable payload identity/revision and provenance, and every reservation. Sequences cannot decrease or describe different content at the same value. Ticket states cannot regress and released reservations cannot become active again. The known format-1 to format-2 provenance upgrade is permitted; an older format cannot replace it. An unsupported future format remains blocked.

## Pristine setup aliases

Google's initial recovery can rename a pristine duplicate after a client has observed it. The only permitted automatic identity change requires reading that exact former object under its deterministic archived name, with bracketed revision checks, and confirming it is byte-for-byte equivalent as a parsed control record to the saved pristine checkpoint. It must have no history, tickets, reservations or prior mutations. The selected replacement must extend that checkpoint. A forged alias, changed legacy identity or any prior root activity blocks rebinding. Neither root is deleted.

During duplicate recovery, the known root's contents are checked against its checkpoint before renaming candidates. An established bound identity must be present in the complete candidate list. Multiple active roots remain protected and blocked; this increment does not choose between them.

## Limits and next gates

This is a local anchor after first observation. A new profile/device starts without that observation; externally removing local application data also removes its anchor. Production namespace discovery, migration/rollback, resolution of multiple active roots and safe retention remain separate protocol gates. A checkpoint is not permission to delete cloud data or to reset an uncertain namespace. Existing schema 1/2 files and all earlier test namespaces stay protected.

Regression coverage includes persistent restart, same-ID rollback, same-sequence changes, replacement/missing roots, unknown local state, storage/lock failures, context mismatch, pending-ticket/reservation preservation, pristine alias validation and simultaneous initialization with separate persisted checkpoints. Live Windows validation must use the new binary and its matching QML revision.

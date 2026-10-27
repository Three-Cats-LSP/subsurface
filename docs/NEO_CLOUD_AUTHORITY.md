# Local history authority checkpoints

Implemented and Windows-validated at `8b620b6bd` on 2026-10-27. This is another development-gated increment of [coordinated cloud history](NEO_CLOUD_HISTORY.md). It does not migrate ordinary synchronization or enable cleanup.

## Stored state and boundaries

Before using a history root, native Neo records its exact provider object identity and complete validated control record in local application data under `neo-history-authority/`. The filename hashes the settings/profile scope, provider, verified account hash and namespace epoch. The checkpoint contains version/ticket/reservation metadata, never XML payloads, account email addresses or OAuth credentials. OAuth tokens retain their existing secure-store path.

Writes use `QSaveFile` atomic replacement and a per-checkpoint `QLockFile`. A competing process, corrupt/unsupported checkpoint or storage error stops the operation without replacing the checkpoint. Each observation reloads the checkpoint under the lock, including before root writes. A missing or replaced established root is not silently recreated or adopted. Disconnect and grant rejection preserve these recovery checkpoints; account and namespace scopes remain distinct.

A later root must preserve the observed history prefix, every upload ticket, immutable payload identity/revision and provenance, and every reservation. Sequences cannot decrease or describe different content at the same value. Ticket states cannot regress and released reservations cannot become active again. The known format-1 to format-2 provenance upgrade is permitted; an older format cannot replace it. An unsupported future format remains blocked.

## Pristine setup aliases

Google's initial recovery can rename a pristine duplicate after a client has observed it. The only permitted automatic identity change requires reading that exact former object under its deterministic archived name, with bracketed revision checks, and confirming it is byte-for-byte equivalent as a parsed control record to the saved pristine checkpoint. It must have no history, tickets, reservations or prior mutations. The selected replacement must extend that checkpoint. A forged alias, changed legacy identity or any prior root activity blocks rebinding. Neither root is deleted.

During duplicate recovery, the known root's contents are checked against its checkpoint before renaming candidates. An established bound identity must be present in the complete candidate list. Multiple active roots remain protected and blocked; this increment does not choose between them.

## Staged handoff proofs

The later [handoff-proof foundation](NEO_CLOUD_HANDOFF_PROOFS.md), `cd3ffbc6c`, can verify a source's explicit destination receipt and preserve exact checkpoint bytes in an immutable recovery file under the same process lock. It leaves the active binding unchanged and does not broaden pristine rebinding. Its 377-case native Linux/Windows protocol run passed; current packaged validation is recorded separately. No application action invokes proof staging or active-root resolution yet.

## Limits and next gates

This is a local anchor after first observation. A new profile/device starts without that observation; externally removing local application data also removes its anchor. Production namespace discovery, migration/rollback, resolution of multiple active roots and safe retention remain separate protocol gates. A checkpoint is not permission to delete cloud data or to reset an uncertain namespace. Existing schema 1/2 files and all earlier test namespaces stay protected.

Regression coverage includes persistent restart, same-ID rollback, same-sequence changes, replacement/missing roots, unknown local state, storage/lock failures, context mismatch, pending-ticket/reservation preservation, pristine alias validation and simultaneous initialization with separate persisted checkpoints. Live Windows validation must use the new binary and its matching QML revision.

## Windows validation, 2026-10-27

Application revision `8b620b6bd9133f388450a75657c7f770723b26ab` passed [Windows build/launch/Clean/Modern/lifecycle](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40490588930), [38 desktop and 35 mobile-target regression suites](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40490286462), [QML](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40490291687) and [Android compilation](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40490593284). CI-only follow-ups added actual native Windows protocol execution, which [passed alongside Linux protocol checks](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40493577806). Android compilation does not cover physical-device behavior.

Thirty-six completed live native stages on both providers covered concurrent initialization/restart, legacy-branch copy/restart, Google pristine-alias rebinding, rollback/missing/replaced-root refusal and recovery, and successful save/restore/restart. Two additional restore attempts were safely refused after reload recalculated canonical XML; explicit saving then allowed restore. Both final local checkpoints exactly matched the four-version cloud root, with restore reservations released. The repaired narrow confirmation footer was captured and its button bounds checked.

The first two Google guard-driver attempts stopped after five successful stages because a newly created replacement fixture's Google inventory version changed from 1 to 2. The exact contents and local checkpoint were unchanged. Exact-ID, conditional recovery preserved both replacement copies; the final native recovery stage passed. This fixture issue was not treated as an application regression or hidden as an uninterrupted pass. Original and failed-attempt evidence remains under `work/native-history-authority-google-8b620-20261027-guards*`.

Successful local evidence is under `work/native-history-initialization-{google,dropbox}-8b620-20261027-*`, `work/native-history-authority-{google,dropbox}-8b620-20261027-guards*` and `work/native-history-{google,dropbox}-8b620-20261027-restore`. All temporary credentials/proxy settings were removed and native processes exited. Independent `post-authority-8b620-20261027` audits reproduced the original 47 Google/nine Dropbox entries, metadata digests, current cloud bytes and original credentials. All 84 history-test objects across old/new namespaces remain protected, including 33 added during this revision's tests. The separate eleven deletion-probe objects described in the retention evidence were newly created solely for that probe and removed there; no application cleanup exists.

The next [read-only competing-branch review](NEO_CLOUD_BRANCH_REVIEW.md) is a separate increment. Its validation must use its own application revision; these completed checks must not be reused as proof that a newer binary passed.

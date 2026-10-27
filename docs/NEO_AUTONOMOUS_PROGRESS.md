# Windows cloud reliability continuation

Updated 2026-10-27 JST. User requested automatic progression without another “next step” command. Work on `modern-ui` in the existing checkout. Preserve unrelated local files, original profiles/credentials, and every existing schema 1/2 cloud object. Windows only; physical Android and a second live Dropbox account remain unavailable. Never publish a release or enable deletion of existing cloud logs as part of this continuation.

## Current increment

Pristine history-root recovery is implemented, with native regression cases added. Validation is pending on the new revision; the previous validated application revision is `9c3d3dcf0`. See [history protocol and prior evidence](NEO_CLOUD_HISTORY.md).

- Google duplicate roots are fully validated and only pristine sequence-zero roots can be conditionally renamed. An active root wins over pristine ones, including an earlier delayed create. Multiple active roots and unknown data remain blocked and preserved.
- Initialization retries conditional conflicts by rereading current state, bounded to eight retries. Pending tickets/snapshots are retained. Ordinary save/restore still stop on conditional conflicts.
- Read-only history loading and legacy/payload duplicate lookup never invoke this recovery.
- Tests cover simultaneous initialization on both providers, pending activity, invalid identity/metadata, competing active roots, read-only behavior, registration racing a rename, lost replies, cancellation/restart and unchanged legacy files.

Next: run native regression and Windows package checks; fix failures; perform isolated live Windows setup/restart validation without touching prior namespaces. Record exact revision/run IDs and evidence here. Then continue legacy-branch preservation/resolution, durable authority binding and guarded automatic migration, followed by remaining retention gates in [the design](NEO_CLOUD_RETENTION.md). Do not treat pristine-root recovery as full authority convergence.

## Continuation mechanics

A heartbeat named **Continue Subsurface Neo Windows reliability**, ID `continue-subsurface-neo-windows-reliability`, was created in the current Codex chat at a 30-minute interval. It should resume this work and inspect current git/CI/evidence before changing anything. Stay quiet when unchanged or blocked on the same user input; report meaningful completion, failures or needed action. Pause it when authorized Windows work is complete or only user-dependent gates remain.

Local live helpers and previous evidence remain in untracked `work/`. Credentials must never be printed or committed. New tests need fresh epochs; original profiles and previous test objects are not disposable targets for destructive operations.

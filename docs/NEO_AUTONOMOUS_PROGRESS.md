# Windows cloud reliability continuation

Updated 2026-10-27 JST. User requested automatic progression without another “next step” command. Work on `modern-ui` in the existing checkout. Preserve unrelated local files, original profiles/credentials, and every existing schema 1/2 cloud object. Windows only; physical Android and a second live Dropbox account remain unavailable. Never publish a release or enable deletion of existing cloud logs as part of this continuation.

## Current increment

Pristine history-root recovery and explicit legacy-branch copying are implemented, with native regression cases added. Validation is pending on the new revisions; the previous validated application revision is `9c3d3dcf0`. See [history protocol and prior evidence](NEO_CLOUD_HISTORY.md).

- Google duplicate roots are fully validated and only pristine sequence-zero roots can be conditionally renamed. An active root wins over pristine ones, including an earlier delayed create. Multiple active roots and unknown data remain blocked and preserved.
- Initialization retries conditional conflicts by rereading current state, bounded to eight retries. Pending tickets/snapshots are retained. Ordinary save/restore still stop on conditional conflicts.
- Read-only history loading and legacy/payload duplicate lookup never invoke this recovery.
- Tests cover simultaneous initialization on both providers, pending activity, invalid identity/metadata, competing active roots, read-only behavior, registration racing a rename, lost replies, cancellation/restart and unchanged legacy files.
- Explicit **Copy changed cloud log** confirms and publishes a fresh copy of the legacy branch as the newest history version, retaining all earlier versions and leaving the local log intact. Format 2 persists source provenance; only verified publication acknowledges the legacy fingerprint. Tests cover failures, restart, stale writes, changed legacy input, idempotency, save and restore after copying.

The first full regression at `6a7709d61` exposed an overbroad initialization retry on a payload revision conflict and a test-harness assumption about stale rename requests. Both were corrected in `bed63eeee`. `f6fd63de2` adds a small Qt-only history protocol job before the full native regression to accelerate feedback. Do not report the earlier failed run as a pass. Windows packages and current full regression still need checking.

### Active verification checkpoint

Application revision **`a0f5b0d4120950008e6f29f3e68683be1a99ac47`** contains both increments. Its Qt-only history protocol job passed (12.34 seconds), as did QML parsing/contracts. As of 2026-10-27 10:00 UTC:

- [Current cloud regression](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40483566947): protocol passed; full desktop/mobile-target regression still building. Protocol job ID `122119629097`; full job ID `122120156512`.
- [Current Windows package](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40483567013): QML/documentation passed; Windows dependencies still building. Main job ID `122119808441`. Its launch, Clean/Modern, lifecycle and packaging results are not yet known.
- [QML](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40483566852): passed. [Planner](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40483567035) and [Android compilation](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40483566848): still running. Android compilation is not device validation.
- Earlier `f6fd63de2` [regression](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40483247048) passed protocol and desktop regression and is building the mobile target. It does not include legacy-branch copying. Superseded failing runs are retained as evidence; most redundant in-progress runs were cancelled.
- No live cloud files were created or changed this turn. Only in-memory credential refresh was tested; both providers succeeded. The live driver below is prepared and Python syntax-checked, but not yet executed.

After successful current Windows packaging, download the portable artifact into a new `work/` directory. Run `python work/test-native-history-initialization.py <google-drive|dropbox> <new-exe-path> <unique-tag> --ref a0f5b0d4120950008e6f29f3e68683be1a99ac47`. It starts Clean/Modern together with two isolated profiles, preserves pristine Google roots by rename, validates restart in both styles, compares all pre-existing cloud inventory/credentials/baselines and cleans exact temporary credentials/proxy values. `--unseeded` additionally exercises Google setup without prepared duplicate roots. `--legacy-branch` instead seeds a synthetic older history version in a fresh namespace and validates the real Clean copy/confirmation flow and Modern restart; it does not write the actual legacy manifest. Inspect wide/narrow result captures and the branch confirmation captures. All test objects are retained. Always use fresh tags after a failed driver attempt; diagnose failures without deleting namespaces or widening targets.

The driver records the exact QML source revision with `git archive`, so later source changes cannot silently change the tested UI. If the application revision changes, use its own `--ref` and corresponding package. Do not use an older package to claim the new code passed. Update this checkpoint and the roadmap with actual results, then proceed automatically to the next independent gate.

Next: run native regression and Windows package checks; fix failures; perform isolated live Windows setup/restart validation without touching prior namespaces. The prepared untracked driver is `work/test-native-history-initialization.py`; it seeds two pristine Google roots in a fresh epoch and starts Clean/Modern together. Original credentials refresh successfully without consent as checked this turn. Live legacy-branch tests must not overwrite the original legacy manifest: use isolated synthetic inputs/test routing or a separately authorized namespace. Record exact revision/run IDs and evidence here. Then continue durable authority binding, two-active-root preservation/resolution and guarded automatic migration, followed by remaining retention gates in [the design](NEO_CLOUD_RETENTION.md). Do not treat pristine-root recovery as full authority convergence.

## Continuation mechanics

A heartbeat named **Continue Subsurface Neo Windows reliability**, ID `continue-subsurface-neo-windows-reliability`, was created in the current Codex chat at a 30-minute interval. It should resume this work and inspect current git/CI/evidence before changing anything. Stay quiet when unchanged or blocked on the same user input; report meaningful completion, failures or needed action. Pause it when authorized Windows work is complete or only user-dependent gates remain.

Local live helpers and previous evidence remain in untracked `work/`. Credentials must never be printed or committed. New tests need fresh epochs; original profiles and previous test objects are not disposable targets for destructive operations.

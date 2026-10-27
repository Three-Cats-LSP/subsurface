# Exact-identity conditional updates

Development increment `4eb424e5bf44dd97b0f53864d5ef4823d0b5a198`, 2026-10-27. This supplies a prerequisite for [history-root resolution](NEO_CLOUD_ROOT_RESOLUTION.md), not a complete resolver or production migration. Ordinary synchronization/history still use their existing entry points. No fence publication, authority rebinding, retirement or deletion action is exposed.

## Native adapter

`CloudSyncFileStore::uploadIdentity()` requires an explicit provider object ID and nonempty expected revision. It rejects invalid scope/identity arguments before sending a request; it has no create mode or name fallback. Google uses the existing v2 File-ETag media update. Dropbox sends its exact `id:` in `CommitInfo.path`, strict revision-update mode and `autorename: false`; it also checks returned ID/name/revision before accepting an acknowledgement. Missing or malformed acknowledgement metadata is an uncertain outcome, not evidence that nothing changed. Callers must confirm exact identity, expected name, revision and body afterward.

The [official Dropbox specification](https://github.com/dropbox/dropbox-api-spec/blob/main/files.stone) supports `WritePathOrId` for uploads and strict revision conflicts. A native integration must use the new explicit adapter rather than assuming that the older path-based `uploadConditional()` honors Dropbox's optional file-ID argument. Existing entry points retain their behavior in this increment.

`NeoCloudIdentityProbe` is a separate developer executable built from the actual native adapter. Credentials enter through anonymous stdin, never command arguments or files. Its envelope admits only newly named `...-exact-id-...json` synthetic fixtures; writes require an exact-ID/name/hash/revision read first, and a second exact-ID read confirms the update. It never creates, renames or deletes an object. The external driver verifies the authorized disposable account and owns fixture creation. The utility is not part of the application UI or a release.

## Completed native regression

[Protocol run `40523428439`](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40523428439) passed **299 Qt cases** on Linux and Windows, including 45 new adapter rows. Linux CTest took 17.51 seconds; Windows took 32.07 seconds. Coverage includes moved IDs with replacement paths, stale/missing IDs, invalid arguments, cancellation after remote commit, lost responses, throttling/authorization/server failures, malformed acknowledgements and all prior fence/history cases. Every valid adapter attempt sends one update to the exact ID and never performs a name lookup or fallback write.

The standalone target now persists per-case text results; the earlier Windows protocol CTest log omitted Qt stdout. Windows evidence artifact `12742099300` contains the explicit result file. Native probe artifact `12742064320` has SHA-256 `6675df148224b2fdec157a2e14905361f878d63653e3b1df5f1284f4932dc841`, verified before extraction to `work/identity-probe-4eb424e-20261027/app/`.

## Live Windows evidence

Only the existing authorized disposable Dropbox account was used, in two fresh recorded namespaces. No additional account or consent was needed.

- `work/dropbox-exact-id-20261027-capability/`: four actual API cases passed: strict ID/revision update and stale rejection; updating a moved ID without touching its replacement at the former path; missing-ID refusal without creation/fallback; and a committed update whose response was never read, confirmed by exact-ID reread with stale replay rejected. The two newly created probe objects remain retained. This driver is a provider capability probe, not the native application.
- `work/native-identity-adapter-dropbox-4eb424e-20261027/`: the matching compiled native utility passed eight stages: exact read, guarded write with post-read confirmation, stale local revision refusal, wrong local hash refusal, moved-ID/former-name refusal, moved-ID read, moved-ID write preserving the replacement and ordinary-namespace input refusal. All eight processes exited. These stale-input tests stop at the utility's pre-read guard; native provider-CAS races are separately covered by the Qt interleavings and actual API probe above.
- Both sequences verified account identity and complete before/after inventories, preserving every earlier object and the original credential/sync-state blobs. They created four new objects total and deleted none. Independent `post-exact-id-capability-20261027` and `post-native-identity-4eb424e-20261027` read-only audits reproduced the original nine Dropbox objects, exact current manifest/payload and inventory digest `9c114be5c67273de8dd3294c7a3928b2f380d305bd359ac80ff87b04930c149d`.

Dropbox now retains 42 history/probe objects; Google retains its earlier 68, for **110 retained test objects**. Google was not accessed during these live adapter tests. All schema 1/2 objects remain protected; no application deletion adapter exists.

## In-flight application gates

The exact `4eb424e5b` [Windows build/launch/Clean/Modern/lifecycle run `40524438785`](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40524438785) is running. The preceding `e46d02881` [full native regression `40519034489`](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40519034489) and [Windows package `40519027999`](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40519027999) also remain in progress. The new protocol-only run intentionally skipped the full application suites; do not describe it as a completed 38-desktop/35-mobile-target run or as Clean/Modern UI validation.

After those gates, continue confirmed-source verification, resumable Google conditional archival and safe local handoff proof storage, then coordinator convergence and guarded migration. Dropbox move has no revision precondition; its unique-path workflow may preserve a sealed source under its original verified name. Do not substitute read-then-move for conditional archival or infer complete namespace authority from repeated scans.

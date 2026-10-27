# Windows stalled cloud connections

On 2026-10-17, the `fcfc5fbf2` Windows package passed seven native stages for each of Google Drive and Dropbox, using the existing authorized disposable accounts. No application change was needed. The owner has only one Dropbox account, so two-live-account Dropbox validation is deferred; these checks do not replace it. Physical Android testing remains deferred.

## Method and isolation

Each provider used a new temporary `--user` profile, its own Windows Credential Manager entries, automatic sync off, and the established one-dive disposable cloud log. Bootstrap explicitly downloaded that log and assigned the provider as primary. No stage requested an upload, and any upload completion was a driver failure.

A loopback HTTP CONNECT proxy applied only to the temporary profile forwarded opaque TLS traffic during normal operation. During a stall it kept client sockets open, consumed outbound encrypted bytes without forwarding them, and withheld responses. Restoring connectivity closed those stale tunnels and allowed fresh provider requests. The proxy did not decrypt traffic, change system networking, or close the stalled connection to manufacture the observed timeout. Each stalled stage recorded outbound encrypted bytes. Non-provider startup requests were blocked by the proxy.

The native QML driver exercised cold account verification, a file operation after successful account verification, and token refresh after changing only the temporary credential's expiry. Before retrying each timeout, an exact-byte credential check confirmed that both the temporary token record and sync baseline were unchanged. Retry called sync explicitly in the same running process, without browser consent or an application restart.

## Results

| Native stage | Google Drive | Dropbox |
| --- | --- | --- |
| Bootstrap download and primary assignment | Passed | Passed |
| Stalled account verification, then retry | Timeout at 30,165 ms; recovered | Timeout at 30,128 ms; recovered |
| Stalled file operation, then retry | Timeout at 30,024 ms; recovered | Timeout at 30,084 ms; recovered |
| Stalled refresh, then retry | Timeout at 30,061 ms; recovered | Timeout at 30,063 ms; recovered |
| Restart after recovery | Verified account; up to date | Verified account; up to date |
| Disconnect during a stalled operation | Busy state cleared in 65 ms | Busy state cleared in 71 ms |
| Restart after Disconnect | Stayed disconnected | Stayed disconnected |

All three timeout paths released the busy state, reported an error, retained the connection and primary assignment, and returned `up-to-date` with verified identity after retry. The refresh retry used the real provider token endpoint. Disconnect occurred after three seconds of a pending stalled operation; five seconds of observation after restoring connectivity found no late reconnection or completed sync. Both temporary credential entries were absent immediately after Disconnect and after restart.

The file-operation and recovery-restart stages ran in Clean; the other stages ran in Modern. This exercises both styles across the sequence, rather than repeating every stage in each style. All fourteen native processes exited with code zero, and their logs contained no driver failure, QML ReferenceError, TypeError or SyntaxError.

## Data preservation and cleanup

Every stage exported byte-identical canonical local XML for the original synthetic dive. The original profiles' credential and baseline records were unchanged. Final read-only provider checks confirmed the same stable account, cloud manifest ID/content and referenced XML snapshot ID/content for each provider. The checks did not upload, delete or rename cloud files.

Cleanup removed the four exact temporary credential entries and eight temporary proxy override values. An independent exact-key audit confirmed their absence and that all fourteen native test processes had exited. Local test evidence was retained; unrelated local files and the original test profiles were preserved.

Evidence is retained locally in `work/stalled-cloud-fcfc5f-google-run/` and `work/stalled-cloud-fcfc5f-dropbox-run/`, including per-stage logs, exit codes, XML exports and final `result.json` reports. The helpers are `work/test-live-stalled-cloud.py` and `work/audit-live-stalled-cloud.py`. These untracked files contain no token values or OAuth client JSON contents.

## Scope and remaining work

The file and token clients configure a 30,000 ms Qt transfer timeout. Qt defines this as a timeout when no data is exchanged; it does not establish a total duration limit for a transfer that keeps progressing. See [QNetworkRequest transfer timeout](https://doc.qt.io/qt-6/qnetworkrequest.html#setTransferTimeout-1). This pass measures stalled operation paths, including connection/TLS establishment, rather than injecting a pause at a specific decrypted HTTP payload boundary. It does not test slow continuous progress, a physical network adapter outage or a newly arriving cloud revision. Earlier [interrupted upload and simultaneous write checks](NEO_CLOUD_COMMIT_PROTOCOL.md) remain separate evidence.

The existing [Windows build, packaged launch, Clean/Modern and lifecycle checks](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38925358118) and [36 desktop plus 33 mobile-target cloud tests](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38925358189) apply to the tested application code. This documentation-only follow-up adds fourteen live Windows stages and does not rerun those CI suites. Snapshot retention, production OAuth, provider account-settings UI, the deferred Dropbox two-account test and physical device/network validation remain open.

## Slow-transfer follow-up, 2026-10-27

An independent Windows history test now exercises progressing transfers through an opaque TLS proxy in fresh synthetic epochs, with the original schema 1/2 files excluded from mutation. Clean cancellation and subsequent Modern restore/restart passed on both providers at `8b620b6bd`. Dropbox's completed transfer forwarded 468,463 encrypted bytes over 131.583 seconds, with a maximum sample gap of 2.522 seconds. Google's repetitive fixture compressed heavily, so its 41.951-second overall operation did not establish a single long payload read; a high-entropy Google fixture is being tested separately. Do not generalize the Google timing into proof of a long individual request.

The fixture's one trailing separator space was normalized by the existing dive-log parser. A separate final verification established that all other note characters were preserved and the original/restored cloud payloads were byte-identical. The initial aggregate assertion and its correction remain in the evidence. Earlier 3 MiB single-word-note attempts timed out in the native driver after the cloud restore had committed. The Google cancellation driver also used a blocking synchronous network-control query. Those attempts, their local data and every new cloud object were retained; they are not successful test evidence. The revised driver performs cancellation without that network-control dependency and uses a smaller fixture. The pathological large-note behavior remains a separate limit to investigate.

Progressing upload cancellation passed on both providers, but the subsequent throttled uploads stopped before publication under Qt's 30-second inactivity policy. Repeating with a smaller proxy receive buffer still reproduced the timeout. Both current heads remained at three committed versions, and all interrupted tickets were retained. No existing cloud log or credential changed. Evidence: `work/native-history-slow-{google,dropbox}-8b620-20261027-progress-retry/`, including saved prior results, buffered-upload state and logs. The successfully completed download sequence is preserved separately as `download-result.json`; the combined result remains failed until the upload issue is fixed and validated.

**Implemented, validation pending at `4741d6e33da4ee58ebf813e3902fa9be3033046d`:** uploads larger than 64 KiB use a progress watchdog with a 30-second connection/partial-progress inactivity limit and one 120-second response wait once Qt reports the body queued. Positive response-body progress returns to the 30-second inactivity policy. Duplicate/backward progress cannot extend the wait. Smaller uploads, downloads, identity and token requests keep their existing timeout behavior. Cancellation still aborts the exact pending request. History retains its existing 180-second operation deadline; timeout is never treated as proof that a write did not commit.

Twenty-four new native protocol scenarios exercise both providers, timeout before/after progress, unknown totals, delayed acknowledgement, duplicate signals, response stalls, cancellation, retry and the unchanged size boundary. Linux and Windows protocol jobs in [run `40506051030`](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40506051030) passed. Full native suites and [Windows package `40506051541`](https://github.com/Three-Cats-LSP/subsurface/actions/runs/40506051541) are still running. Live success must be established with that matching new package. Qt documents the [inactivity timeout and zero-disable setting](https://doc.qt.io/qt-6.8/qnetworkrequest.html#setTransferTimeout-1) and [upload-progress completion semantics](https://doc.qt.io/qt-6.8/qnetworkreply.html#uploadProgress); this policy and its measured live behavior remain Neo's responsibility.

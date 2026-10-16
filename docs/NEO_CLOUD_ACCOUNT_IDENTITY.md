# Connected cloud accounts

Neo verifies the current Google Drive or Dropbox account before using a new access token for file operations. The check is repeated after app restart and token refresh. Cloud & Sync displays the account email (or name if email is unavailable); cached details are explicitly labelled until checked in the current process. **Check account** refreshes these details without touching cloud files.

## Identity and permissions

Google uses `GET /drive/v3/about?fields=user(permissionId,displayName,emailAddress)` with the existing `drive.appdata` grant. The stable `permissionId` identifies the account. Dropbox uses `POST /2/users/get_current_account` with the existing `account_info.read` grant and compares `account_id`. Display names and email addresses are labels, not account identifiers, so changing them does not discard a baseline.

References: [Google about.get](https://developers.google.com/workspace/drive/api/reference/rest/v3/about/get), [Google User fields](https://developers.google.com/workspace/drive/api/reference/rest/v3/User), [Dropbox current account](https://www.dropbox.com/developers/documentation/http/documentation#users-get_current_account).

Account ID, name and email are saved alongside tokens in the platform credential store. They are not written into cloud manifests, ordinary settings, diagnostic messages, or test reports. Windows uses Credential Manager and the existing profile-specific namespace. Platforms without a secure backend retain credentials in memory, as before. Disconnect removes these local details as well as tokens and sync state; it does not revoke the remote grant.

## Account changes and recovery

A different stable account ID clears the previous baseline, primary/backup assignments, and last-sync timestamp before the new identity is saved. The current sync stops and the local dive log remains intact. The user can explicitly sync again to compare the new account's cloud log and establish a new baseline, then choose its primary/backup role. No automatic merge or overwrite is performed to resolve the account change.

An existing credential from an older Neo build can acquire its account identity without losing its baseline. If a new authorization is accepted while the previous account ID is unknown, the old baseline and assignments are cleared conservatively. Native-token renewal follows the same rule because the account chooser can return a different account.

A failed or malformed identity lookup stops the operation before proceeding with that token. Transient errors retain the connection and baseline for retry; HTTP 401 uses the existing token refresh/reconnect recovery. Requests have a 30-second inactivity timeout, a 35-second total deadline, a 64 KiB response bound, and do not follow redirects. Disconnect cancels pending identity work; late replies cannot restore the connection.

## Windows validation, 2026-10-17

At `fcfc5fbf2`, both authorized disposable providers passed ten native Windows stages: initial identity lookup and cloud download, restart, refresh after local expiry, updating a cached display label for the same account ID, refused-proxy lookup failure, online recovery, a deliberately mismatched previous account ID, explicit cloud-copy recovery, local Disconnect, and restart while disconnected.

The real account ID/name/email returned by each provider were compared in memory with the saved Credential Manager fields. Native QML checks confirmed the email label was present, wrapped, and used plain text. Both mismatch stages stopped before file requests, removed the old baseline and assignments, preserved the disposable local dive, and required an explicit retry. All ten stages passed for both providers. The fixed remote manifests and original profile credentials/baselines remained unchanged; the four exact temporary credential entries and temporary proxy overrides were removed. Evidence remains local under `work/account-identity-fcfc5f-google/` and `work/account-identity-fcfc5f-dropbox/`, with no account values or tokens in the result reports.

The mismatch test seeds a synthetic previous account ID into the temporary credential and verifies it against the real account endpoint. It exercises the native mismatch/recovery path without changing the provider account or consent. A subsequent [real provider-grant revocation and reconnection pass](NEO_CLOUD_REVOCATION_TESTING.md) passed for both providers on the same Windows binary. Google then passed the two-real-account test below; switching between two different live Dropbox accounts remains a separate test gate. Physical Android runtime tests remain deferred.

Thirty new native regression cases cover both identity response formats, legacy credentials, same-account label changes, different IDs, new consent/native renewal, token refresh, cancellation/late replies, HTTP 401 with and without refresh, server failures, redirects, malformed/missing IDs, and oversized responses. Existing transaction, simultaneous-writer and interrupted-recovery cases remain in the same suite.

[Windows build, packaged launch, Clean/Modern and lifecycle checks](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38925358118), [desktop and mobile-target cloud regression](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38925358189), and [planner regression](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38925358123) passed. These results do not close the remaining public OAuth, retention or physical-device gates.

## Two real Google accounts, 2026-10-17

The same `fcfc5fbf2` Windows package passed an A/B/A authorization sequence with two actual Google accounts. The owner completed browser consent; distinct stable account IDs were verified rather than seeded. A new UUID-scoped Neo profile held a copy of account A's authorized credential and the disposable one-dive log. The original profile's credentials, sync state and settings were not modified.

| Native stage | Result |
| --- | --- |
| Initialize A | Verified A, downloaded its disposable cloud log and established a primary baseline |
| Authorize B while connected to A | Detected the changed account, cleared A's baseline, primary assignment and last-sync state; displayed B's verified identity |
| Restart on B | Retained B's identity with no baseline or assignment; automatic sync enabled but no file operation occurred during the observation period |
| Authorize A while connected to B | Detected the reverse change and cleared a temporary backup assignment; local dive remained intact |
| Explicit recovery on A | Downloaded A's existing cloud copy to establish a fresh baseline and primary role |
| Restart on A | Verified A and returned `up-to-date` |
| Local Disconnect | Removed the temporary profile's connection and sync state |
| Restart disconnected | Retained the local dive and stayed disconnected with blank identity and assignments |

Both account changes stopped before file synchronization. The driver also checked that `syncPrimaryIfReady()` returned false with automatic sync enabled after each change. The B restart observed no transfer for 6.5 seconds after identity verification, covering the delayed startup hook. The new identity label was present as plain text. The forward switch used Modern and the reverse switch/recovery used Clean.

Every successful stage exported canonical XML byte-identical to the original disposable dive. B's App Data file inventory, including IDs, modification times, sizes and checksums, was unchanged across its identity/restart checks; no B log content was downloaded. A's original manifest and referenced XML retained their IDs and contents. Both exact temporary Credential Manager entries were absent after cleanup, original credentials and baseline were byte-identical to their pre-test values, and all test processes exited. No cloud upload or remote grant revocation was performed.

This exercises the native OAuth reauthorization entry point while connected, including preservation of the old identity until the new account is verified. The ordinary UI currently uses Disconnect then Connect; this pass did not add a dedicated account-switch button or migrate a log into B's storage. An initial consent selected A again and retained A's baseline; that attempt was excluded from the cross-account results and repeated with B.

Evidence remains local under `work/google-two-accounts-fcfc5f/`, with eight completed stages and `passed: true` in `state.json`. The successful forward-consent evidence is in `switch-b-other-account/`; `switch-b/` retains the initial same-account attempt. The helper is `work/test-google-two-accounts.py`. Reports contain account hashes rather than account labels or tokens. No application fix or new binary was required, so the existing CI results above apply. Two-live-account Dropbox validation, provider account-settings UI, public OAuth provisioning, snapshot retention and deferred physical Android testing remain open.

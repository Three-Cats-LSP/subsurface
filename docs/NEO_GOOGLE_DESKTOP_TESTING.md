# Google Drive desktop development testing

The Windows consent test on 2026-10-10 reached Google's browser consent but failed at token exchange with `client_secret is missing`. Publishing the consent screen does not supply this missing client credential. Keep the application in Testing while validating the implementation.

## Google project setup

1. Select the Neo Google Cloud project that owns the existing desktop client. Enable the Google Drive API.
2. In Google Auth Platform, configure Branding and Audience. For an external app in Testing, add the disposable test account to Test users.
3. Configure only `https://www.googleapis.com/auth/drive.appdata` for this integration. This gives Neo its private application folder, not access to the user's ordinary Drive documents.
4. In Clients, use the JSON saved when the **Desktop app** client was created, or create a dedicated Desktop test client if that file is unavailable. Download the JSON immediately in the creation dialog; Google only makes new client secrets available at creation time. Do not use Web or Android client JSON for Windows, and preserve existing clients.
5. Store the file outside the repository. Never paste it into chat, test logs, or source control.

References: [Google consent setup](https://developers.google.com/workspace/guides/configure-oauth-consent), [installed application authorization](https://developers.google.com/identity/protocols/oauth2/native-app), [Drive application data](https://developers.google.com/workspace/drive/api/guides/appdata).

## Local launch

In the shell used to launch the development build, set `SUBSURFACE_NEO_GOOGLE_CLIENT_FILE` to the absolute JSON path. Neo reads the `installed.client_id` and `installed.client_secret` fields. If `SUBSURFACE_NEO_GOOGLE_CLIENT_ID` is also set, it must match the JSON. Authorization and token endpoints remain fixed in Neo; endpoints from the JSON are not trusted or followed.

Use an isolated `--user=neo-google-test-<unique-id>` profile and a disposable local log. Windows credentials are namespaced by that profile. Keep automatic sync off for initial testing. Connect through the system browser, establish an empty-cloud baseline, verify no-change sync, restart with the same profile and configuration, then test refresh and offline/retry. Preserve any existing remote payload or conflict for inspection.

This local JSON option is a development path. Ordinary users must not be required to create Google projects or download developer credentials. Final public credential provisioning, consent-screen publication/verification requirements, account identity, revocation, and two-device conflict behavior are separate release gates. Never bundle a Web application's confidential secret into Neo.

## Live test status, 2026-10-16

A new Desktop client JSON was supplied locally and recognized by the Windows `02c496ba8` package. The isolated profile contained one synthetic dive, with automatic sync disabled. The user completed browser consent. Neo then reported an OAuth callback state mismatch; no provider connection or cloud upload was confirmed.

Review found that the loopback listener passed every browser request into the OAuth redirect handler and parsed incomplete TCP reads. Extra requests such as `/favicon.ico` could clear the active authorization before the token response arrived. Commit `3bd491365` buffers complete bounded requests, ignores unrelated paths/states, prevents duplicate code exchanges, and changes the browser response to say authorization was received rather than claiming the provider is connected. A local-socket regression covers fragmented requests, favicon/stale callbacks, duplicates, and recovery after a token rejection; it passed in both desktop and mobile CI.

Windows, Android, cloud (desktop and mobile), planner, and QML CI all passed for `02c496ba8`. Builds for the callback fix were initially blocked: push created no runs, and both manual Windows/cloud workflow dispatches returned HTTP 422, `Actions has been disabled for this repository`, despite enabled repository permissions and active workflows. Reapplying `enabled=true` with the user's approval did not resolve the rejection.

Later on 2026-10-16 the owner enabled Actions and both manual dispatches passed for `16c81d716` (which includes the callback fix): [Windows](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38885409343) and [cloud regression](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38885404181). Windows compilation, packaged launch, and native Clean/Modern checks passed. Cloud regression passed all 36 desktop and 33 mobile tests, including `TestCloudSyncManager`. The previous support draft is resolved.

The rebuilt Windows package was tested against Google Drive using the same isolated profile and one synthetic dive (#1, 18 m, 20 minutes, 2026-10-16). Browser authorization completed, the provider connected, and the first sync uploaded the log. A second sync reported `up-to-date`. After closing and restarting Neo with the same profile, the provider remained connected without another browser authorization. An explicit download of the established test log reported `downloaded` and preserved the dive count, number, depth, duration, and date. Windows Credential Manager contained both access and refresh tokens under the isolated profile key; token values were not printed or copied into test evidence.

A connection failure was induced using an unreachable loopback HTTP proxy in this test profile only. Sync displayed `Proxy connection refused`, released the busy state, and re-enabled Sync now. After closing Neo, the original profile proxy settings were restored. The saved access-token expiry was then set into the past for this isolated credential only, preserving its access and refresh token values. On restart, the synthetic dive remained intact. Manual sync refreshed authorization against Google and returned `up-to-date`; the secure credential now had a future expiry and still contained a refresh token, without another browser sign-in.

| Live Windows check | Result |
| --- | --- |
| Browser authorization and initial upload | Passed |
| Repeat sync with no changes | Passed (`up-to-date`) |
| Credential and dive-log persistence after restart | Passed |
| Download and apply the established disposable cloud log | Passed (`downloaded`), synthetic dive fields preserved |
| Failed connection releases controls and preserves local data | Passed using a refused proxy connection |
| Recovery after restoring profile networking | Passed (`up-to-date`) |
| Refresh using a locally expired access-token timestamp | Passed; future expiry saved and refresh token retained |

These tests use the rebuilt native executable with a QML test driver to isolate the profile, load the synthetic fixture, and report outcomes. Packaged launch and Clean/Modern validation are separate CI checks above. On 2026-10-16, the `a0de69ed4` Windows package also passed real Google automatic startup sync with the window hidden, a refused loopback-proxy failure that released the busy state, and automatic recovery after restoring the profile's network settings. Every stage preserved the one synthetic dive. The driver observed the lifecycle without calling sync. Automatic sync and proxy settings were restored after each run.

The final `d9b4122ea` Windows package also passed automatic startup sync after the Google test access token expired naturally. No manual sync or browser sign-in was invoked. Neo reported up to date with the original synthetic dive, and the isolated secure credential retained its refresh token and acquired a future access-token expiry. [Windows launch/Clean/Modern/lifecycle validation](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38895503177) and [both native cloud regression targets](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38895503195) passed at that code revision.

At that stage, Google foreground sync, actual adapter disconnection, stalled transfers, account revocation, cross-device conflicts, and physical Android runtime tests remained open. Later Windows foreground, revocation and transaction evidence is recorded below. Dropbox evidence is recorded separately in [Dropbox desktop testing](NEO_DROPBOX_DESKTOP_TESTING.md). The owner requested Windows-only live tests for now. Public credential provisioning and consent-screen production readiness remain open; see [OAuth release readiness](NEO_OAUTH_RELEASE_READINESS.md). Milestone 13 is not complete. No ordinary user log or unrelated local file was changed.

## Rejected-token recovery, 2026-10-16

The `430a76bc9` Windows package passed two additional live Google recovery scenarios in fresh disposable profiles. An expired synthetic access token with an invalid refresh grant was rejected by Google's real token endpoint. Neo released the busy state, cleared that profile's saved tokens, sync baseline and primary assignment, and offered a new connection. Restart retained the one synthetic dive, remained disconnected, and did not automatically retry. Windows Credential Manager checks confirmed the isolated token and baseline entries had been removed.

A separate profile used a synthetic invalid access token with a future local expiry and a copy of the authorized disposable account's refresh token. Google returned HTTP 401. Neo discarded the rejected access token while retaining the renewable connection. The next explicit download refreshed authorization and retrieved the established one-dive cloud log. Restart passed automatic sync with the dive intact. The extra profiles' credentials were removed after testing. These checks use real provider rejection responses; they do not claim revocation through Google's account settings.

[Windows launch/Clean/Modern/lifecycle checks](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38903824102) and [all 36 desktop plus 33 mobile-target cloud tests](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38903824200) passed at this revision. Local Disconnect remains distinct from remote provider-grant revocation.

The original disposable Google profile then passed local Disconnect, restart into the disconnected state, owner-completed browser consent, explicit cloud download to establish the new baseline, and another restart with `up-to-date` sync. The synthetic dive survived every stage, the Cloud & Sync page showed Connected/Primary after authorization, and fresh access/refresh credentials with a future expiry were saved. Automatic sync remained off. No test app was left running.

## Simultaneous writes and interrupted uploads, 2026-10-16

The `ed8a15fd4` native Windows package passed live tests using additional isolated profiles of the authorized disposable account. Schema 1 migration published a schema 2 manifest while retaining the legacy XML. Two profiles then read the same baseline, added different synthetic dives, and paused after uploading their separate XML snapshots. Releasing both manifest commits together produced exactly one successful upload and one conflict. The losing local log survived restart and another sync still reported conflict; explicitly choosing the cloud copy resolved it.

A larger synthetic dive made it possible to interrupt a transfer while bytes were still in flight. A localhost CONNECT proxy used only by that test profile forwarded 65,536 encrypted outbound bytes to the provider, then cut the connection. It did not decrypt traffic or change system networking. Neo released the busy state and retained all local dives. A separate reader downloaded the previous complete cloud log. The profile proxy settings were removed immediately afterward.

The app was also stopped after its XML snapshot reached the provider but before manifest publication. A separate reader again received the previous log. Restart retained the pending local addition, and retry uploaded it successfully; the reader then obtained the complete new log. Finally, stopping after the manifest reached the provider but before local confirmation was recorded recovered on restart as `up-to-date`, with the committed data readable from the separate profile. Simulated native tests additionally cover a lost HTTP success response.

Checks used the complete dive model and exported canonical XML, including expected numbers and disposable notes. Both providers were restored to the original one-dive disposable baseline. Extra profiles' access/refresh and sync-state credentials were removed, their local evidence was retained, and the original test profiles and unrelated files were preserved. Old and abandoned revision snapshots remain available for recovery; no blanket remote cleanup was performed. Both original authorized test profiles subsequently passed restart and `up-to-date` sync with the new Windows package. An exact-key audit confirmed all 12 temporary credential entries and all temporary proxy overrides were absent.

[Windows build, launch, Clean/Modern and lifecycle checks](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38909378239), [36 desktop and 33 mobile-target regression tests](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38909378266), and [planner regression](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38909378290) passed at `ed8a15fd4`.

See the [commit protocol and compatibility limits](NEO_CLOUD_COMMIT_PROTOCOL.md). All syncing clients must be upgraded for schema 2. First-time Google initialization recovery is covered by the follow-up below; snapshot retention policy remains open. Physical Android validation remains deferred at the owner's request.

## Automatic first-time setup recovery, 2026-10-16

Code `74c9a3f1b` recovers competing initial Google manifests without provider-console repair. The live Windows harness held both clients after XML upload, released first-manifest creation together, then held both after their manifest uploads. Provider reads confirmed two distinct manifest IDs before either client began post-commit verification.

| Windows check | Result |
| --- | --- |
| Different initial logs, two recovering clients | One upload completed, one conflict; both local logs retained |
| Losing client restart and retry | Original local log survived; ordinary initial-choice flow remained available |
| Explicit cloud choice and separate reader | Both obtained the elected cloud log |
| Delayed first manifest | Automatically archived; existing active manifest ID retained |
| Process killed partway through eight-candidate recovery | Restart completed recovery; all candidate contents retained |
| Identical initial logs | Both completed, one active manifest; both reported up to date after restart |
| Recovery copies | Original manifest bytes and immutable XML retained; no blanket cleanup |
| Original disposable cloud | Original manifest and legacy XML restored by exact ID and byte content |

Both identical clients also saved exactly the same canonical baseline. Three subsequent native restarts against an established head each recovered a newly injected initial candidate and remained up to date.

The harness parked only the verified original disposable manifest and legacy XML while exercising an empty namespace. It restored them in cleanup, removed its temporary credential keys, and terminated only its own test processes. Original test credentials and unrelated local files were preserved. Evidence is under `work/google-initial-74c9a3-final/` (untracked); no tokens or OAuth JSON contents are recorded.

[Windows build, launch, Clean/Modern and lifecycle checks](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38917564891) and [native cloud regression](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38917564862) passed. The regression suite also covers stale rename rejection, failed/lost rename responses, cancellation, incomplete metadata, invalid manifests, established competing branches, timestamp ties, reversed list order, and bounded retries. Recovery is deliberately limited to initial candidates; it does not silently merge dive logs or archive an established divergent branch. Physical Android testing remains deferred.

An earlier live attempt terminated once in `ucrtbase.dll` with `0xc0000409` during the delayed-initial stage. Cleanup restored the original cloud files. This did not reproduce under exception tracing, in the complete untraced rerun, or in three additional native restarts. Its cause remains unidentified and is recorded for Windows stability follow-up; this cloud change is not claimed to fix that abort. A separate harness observation request received Google HTTP 500; the harness now retries transient read-only observation failures. These earlier attempts are retained in `work/google-initial-74c9a3/` and `work/google-initial-74c9a3-retry/`.


## Connected-account identity, 2026-10-17

The `fcfc5fbf2` Windows package passed ten native identity/recovery stages for this provider, including persisted account details, token refresh, same-account label changes, refused-connection recovery, a seeded previous-account mismatch, explicit recovery, and local Disconnect/restart. The mismatch stopped sync and cleared the previous baseline/assignments while retaining the disposable dive. Original profile credentials and cloud manifests were unchanged; temporary credential entries and proxy overrides were removed. See [account identity behavior, exact evidence and remaining limits](NEO_CLOUD_ACCOUNT_IDENTITY.md). The mismatch was injected locally; switching between two different live provider accounts remains a separate test. Real grant revocation was tested subsequently as recorded below.

## Real grant revocation and reconnect, 2026-10-17

The same `fcfc5fbf2` Windows package passed real provider-side grant revocation through the provider's API, rejection of the revoked access and refresh tokens, local credential/identity/baseline cleanup, and restart while disconnected. The owner then completed fresh browser consent. An explicit cloud download restored the baseline; another restart verified the account and returned `up-to-date`.

All five native stages retained byte-identical canonical local XML. A final read-only comparison confirmed the same stable account, manifest ID/content and immutable XML snapshot ID/content. The original disposable profile ended connected with its provider as primary and automatic sync off; all test processes exited. No code change was required. See [exact revocation method, results and limits](NEO_CLOUD_REVOCATION_TESTING.md). Provider account-settings UI and switching between two different live accounts remain untested; physical Android validation remains deferred.

## Automatic foreground sync, 2026-10-17

The same `fcfc5fbf2` Windows package passed the following live checks in both Clean and Modern, using the existing authorized disposable Google profile. The QML driver minimized and restored the real native window, observed actual application activation, and never called a sync method.

| Check | Clean | Modern |
| --- | --- | --- |
| Hidden startup with automatic sync disabled stayed idle | Passed | Passed |
| Enabling automatic sync and returning to Neo synced the established primary provider | Passed | Passed |
| Returning with automatic sync disabled made no sync request | Passed | Passed |
| Re-enabling it and returning again synced successfully | Passed | Passed |
| Temporary Google connection failure released the busy state and retained the connection/local log | Passed | Passed |
| Restoring connectivity and returning again recovered automatically | Passed | Passed |

Each enabled activation produced one sync attempt approximately 2.5 seconds after the observed activation event. Each style produced three `up-to-date` results and one expected connection error; the disabled foreground phase remained idle for six seconds. Recovery required no manual Sync now, app restart or new browser consent. Both styles retained the verified account and established baseline.

A loopback CONNECT proxy applied only to this test profile forwarded encrypted Google traffic, temporarily rejected connections, then resumed forwarding within the same app process. It did not decrypt TLS or change system networking. This is controlled connection-failure evidence, not physical adapter-disconnection or stalled-transfer coverage. The cloud log was unchanged during the pass, so it tests activation and retry of an established log rather than arrival of a new remote revision.

Canonical XML exports before and after each style were byte-identical to the original disposable dive export. The saved sync baseline and stable account ID were unchanged. Read-only comparisons confirmed the original cloud manifest and referenced XML snapshot retained their IDs and contents. The exact previous automatic-sync, interface-style and proxy settings were restored; the profile ended connected with Google as primary and automatic sync off. All test processes exited. No native QML ReferenceError, TypeError or SyntaxError occurred in either successful run.

Evidence is retained locally under `work/google-foreground-fcfc5f-clean-retry/` and `work/google-foreground-fcfc5f-modern/`, with final `result.json` reports. The helper is `work/test-google-foreground.py`. An initial harness attempt used an unsupported Windows export-path representation; it was stopped, its settings were restored, and the corrected harness completed both styles. That attempt is retained under `work/google-foreground-fcfc5f-clean/`; it is not counted as a successful application check.

No application fix or new binary was needed. The existing [Windows build, packaged launch, Clean/Modern and lifecycle checks](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38925358118) and [native cloud regression](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38925358189) apply to the tested code. This follow-up adds twelve live lifecycle checks and does not rerun those CI suites. Two-live-account switching, provider account-settings UI, physical network/device tests, snapshot retention and public OAuth remain separate gates. Physical Android validation remains deferred.

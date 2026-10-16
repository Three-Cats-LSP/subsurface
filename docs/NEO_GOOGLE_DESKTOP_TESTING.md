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

Google foreground sync, actual adapter disconnection, stalled transfers, account revocation, cross-device conflicts, and physical Android runtime tests remain open. Dropbox evidence is recorded separately in [Dropbox desktop testing](NEO_DROPBOX_DESKTOP_TESTING.md). The owner requested Windows-only live tests for now. Public credential provisioning and consent-screen production readiness remain open; see [OAuth release readiness](NEO_OAUTH_RELEASE_READINESS.md). Milestone 13 is not complete. No ordinary user log or unrelated local file was changed.

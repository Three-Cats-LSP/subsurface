# Dropbox desktop development testing

## Provider configuration

The owner reviewed the existing Subsurface Neo app in the Dropbox App Console on 2026-10-16. Its app key matches Neo's configured public client ID. The app has Development status, App Folder access, and public clients/PKCE enabled. Development users is currently set to Only you, so the live authorization uses the app owner's account.

The callback addresses registered for this integration are:

- Windows desktop: `http://127.0.0.1:53682/oauth/callback`
- Mobile: `subsurface-neo://oauth/callback`

The current provider requests `account_info.read`, `files.metadata.read`, `files.content.read`, and `files.content.write`. These scopes must be enabled in the app's Permissions tab. Native authorization uses authorization code plus PKCE and requests offline access for refresh tokens. No app secret or manually generated access token is needed for this flow.

References: [Dropbox OAuth guide](https://docs.dropboxapi.com/dropbox-api/docs/oauth), [Dropbox developer guide and production approval](https://docs.dropboxapi.com/dropbox-api/docs/developer-resources/developer-guide).

## Test isolation

Use a unique `--user=neo-dropbox-test-<unique-id>` profile with one synthetic dive and automatic sync initially off. Windows Credential Manager keys are isolated by that profile. Local profile isolation does not create a separate remote Dropbox app folder: if the app folder already contains a log, preserve it and resolve the initial choice explicitly before any upload. Do not overwrite unknown remote data.

The initial manual Windows checks used the native package built from `16c81d716`, with a QML test driver to load the disposable fixture and report results. Later checks identify their code revisions below. The fixture contains dive #1 on 2026-10-16, with a duration of 20 minutes and maximum depth of 18 m. The Google test profile, normal user logs, credentials, and unrelated local files are preserved.

## Live Windows results, 2026-10-16

The owner completed browser consent. Neo reported `connected`, and the initial sync reported `uploaded`. Repeat sync reported `up-to-date`, with the synthetic dive unchanged. Windows Credential Manager retained both access and refresh tokens for the isolated Dropbox profile; token values were not printed or added to the repository.

After closing Neo, only the isolated Dropbox credential's access-token expiry was set into the past. On restart, Neo retained the primary-provider assignment, connected state, and synthetic dive without another browser sign-in. An explicit download refreshed authorization against Dropbox, reported `downloaded`, and preserved the dive count, number, depth, duration, and date. The secure credential contained a future expiry and still retained its refresh token.

A refused loopback proxy connection in the isolated profile produced `Proxy connection refused` and re-enabled Sync now. After closing Neo and restoring the original profile network settings, manual sync returned `up-to-date`, preserving the synthetic dive. No OS network settings were changed.

Automatic startup sync exposed a lifecycle bug: `QMLInterface::setup()` tried to connect `QMLManager::initializedChanged` before QML had constructed that manager. Setup is now attached after `engine.load()` and handles both an already-initialized manager and later initialization. Timers and foreground connections follow the QML manager's lifetime. The `scripts/test-neo-cloud-lifecycle.py` Windows regression uses a hidden window, synthetic credentials, and a refused local proxy; no real account or external provider traffic is needed. Against the previous binary it passed the disabled case and failed the enabled case because startup sync never completed. Both cases passed in the rebuilt `d12a68e81` and `a0de69ed4` Windows CI runs, alongside packaged launch and Clean/Modern validation.

The `a0de69ed4` Windows package also passed live automatic Dropbox startup sync with the window hidden, refused-proxy error recovery with the busy flag released, and automatic no-change sync after network settings were restored. The driver only observed the lifecycle; it never invoked sync. All three stages preserved the synthetic dive, and the isolated profile's original automatic-sync and proxy settings were restored after each run. A separate visible-window check passed automatic sync after minimizing and restoring Neo; the log recorded the additional no-change sync and unchanged dive, and automatic sync was switched off afterward.

Preparing the two-profile Windows conflict test exposed a second bug on the previous binary: a new empty profile downloaded the synthetic cloud dive successfully, but its local log was empty after restart. The downloaded replacement was outside the undo system and never marked dirty for saving. The native lifecycle now connects successful downloads to a dedicated local-cache save, without initiating a legacy Subsurface Cloud upload. The same download/restart scenario passed for both fresh profiles on `a0de69ed4`, with exported canonical XML confirming the expected synthetic notes and dive data.

Review also found that replacement processing rebuilt global search and selection before installing the new log. The old search index and selection are now cleared, and the new log is installed before processing. A regression checks that new notes are searchable, discarded notes no longer match, and current selection points to the replacement. Its first CI run correctly caught incomplete old-index cleanup; the completed fix passed both native cloud test targets at `d9b4122ea`.

## Two-profile Windows conflict check

Two additional isolated profiles used the authorized disposable Dropbox account, with separate local logs and sync histories. Credentials remained in Windows Credential Manager; values were not printed or written to files. After both downloaded the same one-dive baseline and passed restart checks, profile A imported synthetic dive #2 and profile B imported synthetic dive #3. Automatic sync was disabled.

On `a0de69ed4`, A uploaded dives 1 and 2. B's subsequent sync reported a conflict and preserved dives 1 and 3. The native Cloud & Sync page displayed both conflict-resolution choices. A remained up to date with dives 1 and 2. Explicitly choosing the cloud copy in B downloaded dives 1 and 2; after restart, B reported up to date and retained that resolved data. Canonical XML exports verified the dive numbers and disposable notes at every stage.

Afterward, A removed only its known synthetic addition and synced the original one-dive log back to Dropbox. B downloaded that restored baseline using the final `d9b4122ea` Windows executable and retained the expected dive and notes after restart. The two extra profiles' credentials and sync-state credentials were removed; their local evidence was preserved. The original Google/Dropbox test profiles retained their credentials, automatic sync was off, proxy overrides were absent, and no test process remained running. This exercises sequential divergent edits on one Windows machine, not simultaneous writers or the full physical cross-device matrix.

Final code validation: [Windows build and native launch/Clean/Modern/lifecycle checks](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38895503177), [cloud regression](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38895503195), and [planner regression](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38895503169) all passed at `d9b4122ea`. The cloud workflow passed all 36 desktop and 33 mobile-target tests, including `TestCloudSyncManager` in both targets. The Windows package is available in that run's artifacts.

The owner requested Windows tests only for now. Public production approval, provider revocation, physical Android testing, and cross-device conflicts remain separate gates. Automatic sync was turned off after the original-profile test and its network settings were restored.

## Rejected tokens and reconnect, 2026-10-16

The `430a76bc9` Windows package passed two additional live recovery scenarios using separate disposable profiles. A synthetic expired access token with an invalid refresh grant reached Dropbox's token endpoint and was rejected. Neo released the busy state, removed that profile's saved tokens and baseline, cleared the primary assignment, and displayed guidance to connect again. Restart retained the one synthetic dive, stayed disconnected, and did not retry automatically. Windows Credential Manager checks confirmed both isolated credential entries were absent. The previous `d9b4122ea` package failed this same check because it retained the rejected connection and displayed only the provider's malformed-refresh-token error.

In the second scenario, a synthetic invalid access token had a future local expiry, while the fresh profile held a copy of the authorized disposable account's refresh token. Dropbox returned HTTP 401. Neo discarded the rejected access token and retained the renewable connection. The next explicit cloud download refreshed authorization, downloaded the established one-dive log, and survived restart with successful automatic sync. The temporary profiles' credentials were removed afterward; no token values were printed or written to evidence files.

The original disposable Dropbox profile also passed local Disconnect, restart into a disconnected state, owner-completed browser authorization, explicit cloud download to establish a new baseline, and another restart with `up-to-date` sync. Its dive survived every stage. The native Cloud & Sync page showed Ready to connect after Disconnect and Connected/Primary after reconnection. Fresh access and refresh tokens were saved with a future expiry; automatic sync remained off. This exercises local disconnect/reconnect and real endpoint rejection, not revocation through Dropbox's connected-app settings.

Validation at this revision: [Windows launch, Clean/Modern and lifecycle checks](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38903824102), [all 36 desktop and 33 mobile-target cloud tests](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38903824200), and [planner regression](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38903823991) passed. Unit cases also cover disconnect during a pending refresh, authorization exchange, download, and upload, including late replies after cancellation. Physical Android validation remains deferred.

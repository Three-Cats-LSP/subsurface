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

The Windows test uses the native package built from `16c81d716`, with a QML test driver to load the disposable fixture and report results. The fixture contains dive #1 on 2026-10-16, with a duration of 20 minutes and maximum depth of 18 m. The Google test profile, normal user logs, credentials, and unrelated local files are preserved.

## Live Windows results, 2026-10-16

The owner completed browser consent. Neo reported `connected`, and the initial sync reported `uploaded`. Repeat sync reported `up-to-date`, with the synthetic dive unchanged. Windows Credential Manager retained both access and refresh tokens for the isolated Dropbox profile; token values were not printed or added to the repository.

After closing Neo, only the isolated Dropbox credential's access-token expiry was set into the past. On restart, Neo retained the primary-provider assignment, connected state, and synthetic dive without another browser sign-in. An explicit download refreshed authorization against Dropbox, reported `downloaded`, and preserved the dive count, number, depth, duration, and date. The secure credential contained a future expiry and still retained its refresh token.

A refused loopback proxy connection in the isolated profile produced `Proxy connection refused` and re-enabled Sync now. After closing Neo and restoring the original profile network settings, manual sync returned `up-to-date`, preserving the synthetic dive. No OS network settings were changed.

Automatic startup sync exposed a lifecycle bug: `QMLInterface::setup()` tried to connect `QMLManager::initializedChanged` before QML had constructed that manager. Setup is now attached after `engine.load()` and handles both an already-initialized manager and later initialization. Timers and foreground connections follow the QML manager's lifetime. The `scripts/test-neo-cloud-lifecycle.py` Windows regression uses a hidden window, synthetic credentials, and a refused local proxy; no real account or external provider traffic is needed. Against the previous binary it passed the disabled case and failed the enabled case because startup sync never completed. The fix awaits the rebuilt Windows package and live retest.

Preparing the two-profile Windows conflict test exposed a second bug on the previous binary: a new empty profile downloaded the synthetic cloud dive successfully, but its local log was empty after restart. The downloaded replacement was outside the undo system and never marked dirty for saving. The native lifecycle now connects successful downloads to a dedicated local-cache save, without initiating a legacy Subsurface Cloud upload. Live restart verification on the rebuilt binary is pending.

The owner requested Windows tests only for now. Public production approval, provider revocation, physical Android testing, and cross-device conflicts remain separate gates. Automatic sync was turned off after the original-profile test and its network settings were restored.

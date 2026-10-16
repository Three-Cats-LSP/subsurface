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

Review found that the loopback listener passed every browser request into the OAuth redirect handler and parsed incomplete TCP reads. Extra requests such as `/favicon.ico` could clear the active authorization before the token response arrived. Commit `3bd491365` buffers complete bounded requests, ignores unrelated paths/states, prevents duplicate code exchanges, and changes the browser response to say authorization was received rather than claiming the provider is connected. A local-socket regression covers fragmented requests, favicon/stale callbacks, duplicates, and recovery after a token rejection; execution is pending a rebuilt test binary.

Windows, Android, cloud (desktop and mobile), planner, and QML CI all passed for `02c496ba8`. Builds for the callback fix are blocked: push created no runs, and both manual Windows/cloud workflow dispatches returned HTTP 422, `Actions has been disabled for this repository`. The repository API reports Actions enabled and both workflows active. Reapplying `enabled=true` with the user's approval did not resolve the rejection. GitHub Support review is required if this separate disabled state persists. Live sync, refresh, restart, and offline tests remain pending; do not infer completion from the earlier consent page.

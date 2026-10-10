# Google Drive desktop development testing

The Windows consent test on 2026-10-10 reached Google's browser consent but failed at token exchange with `client_secret is missing`. Publishing the consent screen does not supply this missing client credential. Keep the application in Testing while validating the implementation.

## Google project setup

1. Select the Neo Google Cloud project that owns the existing desktop client. Enable the Google Drive API.
2. In Google Auth Platform, configure Branding and Audience. For an external app in Testing, add the disposable test account to Test users.
3. Configure only `https://www.googleapis.com/auth/drive.appdata` for this integration. This gives Neo its private application folder, not access to the user's ordinary Drive documents.
4. In Clients, select the existing **Desktop app** client and obtain its downloaded JSON, or create a dedicated Desktop test client if the original credential cannot be obtained. Do not use Web or Android client JSON for Windows.
5. Store the file outside the repository. Never paste it into chat, test logs, or source control.

References: [Google consent setup](https://developers.google.com/workspace/guides/configure-oauth-consent), [installed application authorization](https://developers.google.com/identity/protocols/oauth2/native-app), [Drive application data](https://developers.google.com/workspace/drive/api/guides/appdata).

## Local launch

In the shell used to launch the development build, set `SUBSURFACE_NEO_GOOGLE_CLIENT_FILE` to the absolute JSON path. Neo reads the `installed.client_id` and `installed.client_secret` fields. If `SUBSURFACE_NEO_GOOGLE_CLIENT_ID` is also set, it must match the JSON. Authorization and token endpoints remain fixed in Neo; endpoints from the JSON are not trusted or followed.

Use an isolated `--user=neo-google-test-<unique-id>` profile and a disposable local log. Windows credentials are namespaced by that profile. Keep automatic sync off for initial testing. Connect through the system browser, establish an empty-cloud baseline, verify no-change sync, restart with the same profile and configuration, then test refresh and offline/retry. Preserve any existing remote payload or conflict for inspection.

This local JSON option is a development path. Ordinary users must not be required to create Google projects or download developer credentials. Final public credential provisioning, consent-screen publication/verification requirements, account identity, revocation, and two-device conflict behavior are separate release gates. Never bundle a Web application's confidential secret into Neo.

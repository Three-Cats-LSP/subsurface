# OAuth release readiness

Engineering review on 2026-10-16. Windows live testing is the current scope; the owner deferred physical Android tests. No production provider settings were changed by this review.

## Current development configuration

| Provider | Observed configuration | Remaining public-release work |
| --- | --- | --- |
| Google Drive | External audience in Testing with named test users; Windows Desktop OAuth JSON loaded locally; live authorization, refresh and XML round trip verified. | Production project/client provisioning, domain ownership and brand verification, production consent configuration, ordinary-user onboarding without a developer JSON file, account identity and revocation testing. |
| Dropbox | Development app, App Folder access, public clients/PKCE allowed, owner account authorized. | Additional-user testing, production approval when applicable, account identity and revocation testing. |

The home page, privacy policy and terms below returned HTTP 200 on the review date. The home page links to both policies. The privacy text describes Google `drive.appdata`, Dropbox App Folder, platform credential storage, local disconnection, and revocation through provider account settings. Reachability does not establish verified domain ownership or formal approval of the policy text.

- [Home page](https://threecats-lsp.com/subsurface-neo/)
- [Privacy policy](https://threecats-lsp.com/subsurface-neo/privacy/)
- [Terms](https://threecats-lsp.com/subsurface-neo/terms/)

## Google production preparation

Google requires separate testing and production projects. Configure the production project's Drive API, Desktop client, branding, support contacts and authorized domain. Verify domain ownership through Search Console and complete the applicable brand verification. Review the actual scope list before publication; `drive.appdata` is classified as non-sensitive, so it does not itself require the sensitive/restricted-scope review. Adding other scopes can change that assessment. See [Google production policy guidance](https://developers.google.com/identity/protocols/oauth2/production-readiness/policy-compliance) and [Drive scope classifications](https://developers.google.com/workspace/drive/api/guides/api-specific-auth).

Neo's Windows release still needs a deliberate credential-provisioning design. The current external Desktop JSON is for development, and ordinary users must not create Google projects or obtain developer credentials. Preserve the distinction between installed/public-client configuration and a Web client's confidential secret; do not put a Web client secret or user tokens in source, settings or binaries.

## Dropbox production preparation

Retain App Folder access and exact registered callbacks. The native code uses authorization-code PKCE and offline refresh, which does not require distributing an app secret. Before inviting testers, verify the requested scopes in the app's Permissions tab and enable additional development users. Follow Dropbox's current production approval process before wider rollout. See the [OAuth guide](https://docs.dropboxapi.com/dropbox-api/docs/oauth) and [developer guide](https://docs.dropboxapi.com/dropbox-api/docs/developer-resources/developer-guide).

## Engineering gates before publication

- Windows automatic startup sync, refused-connection recovery, rejected-refresh-grant cleanup, recovery from HTTP 401 with a future local expiry, and local disconnect/reconnect with restart persistence passed for both providers. Dropbox foreground sync, fresh-profile download/restart and sequential two-profile conflict resolution also passed. Both providers also passed the Windows transaction/interruption checks described below. Continue actual provider-account revocation and Google foreground sync. See the provider testing documents for exact build revisions and scope.
- Implement and test clear connected-account identity. Current `disconnectProvider()` removes local credentials and sync assignments; it does not remotely revoke the provider grant.
- Conditional manifest publication and interrupted-upload recovery passed on Windows for established logs. Resolve automatic recovery for duplicate initial Google manifests, define snapshot retention, and require upgraded clients before wider rollout; see the [commit protocol](NEO_CLOUD_COMMIT_PROTOCOL.md).
- Keep the advertised sync scope accurate: canonical XML and a manifest are synchronized; the separate equipment library, manual collections and planner presets are not.
- Complete physical Android validation when resumed, formal policy approval and the repository's release checklist before public publication.

Publishing a consent screen or obtaining provider production approval does not close these engineering gates.

## Conditional-write probe, 2026-10-16

One uniquely named disposable file was created and removed in each authorized test provider's app storage. Existing Neo XML and manifest files were not changed. Dropbox accepted writer A using the original file revision, rejected writer B using that stale revision with HTTP 409, and retained A's contents. The probe used update mode, `strict_conflict=true`, and `autorename=false`, as described in the [Dropbox SDK API reference](https://dropbox-sdk-python.readthedocs.io/en/latest/api/files.html). Cleanup returned HTTP 200.

The Google Drive v3 probe returned no ETag on either the media or metadata response for its new file. It therefore did not attempt a conditional update or establish a compare-and-swap guarantee. The file was removed successfully (HTTP 204). Follow-up disposable-file probes read the v2 File ETag. A v3 media PATCH did not enforce it, while v2 media PUT with `If-Match` accepted the first writer, rejected the stale writer with HTTP 412, and retained the winner. Bracketing the media read with two v2 metadata reads returned the same ETag. Probe files were removed successfully.

The `ed8a15fd4` implementation publishes immutable XML snapshots through a conditional schema 2 manifest update. Both providers passed live Windows simultaneous-write, mid-upload connection-drop, process-stop, retry, and separate-reader checks. Native regression cases cover first-create races and a lost commit response. These results and the remaining first-initialization/retention limits are documented in the [cloud commit protocol](NEO_CLOUD_COMMIT_PROTOCOL.md), [Google evidence](NEO_GOOGLE_DESKTOP_TESTING.md), and [Dropbox evidence](NEO_DROPBOX_DESKTOP_TESTING.md). Publishing OAuth consent still does not close the remaining release gates.

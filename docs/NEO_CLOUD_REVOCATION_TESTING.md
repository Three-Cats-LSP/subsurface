# Windows provider revocation and recovery

On 2026-10-17, the native Windows package built from `fcfc5fbf2` passed real provider-grant revocation and reconnection for Google Drive and Dropbox. Each test used the existing authorized disposable profile and its one synthetic dive. No application change was needed for these scenarios.

## Scope and method

The harness first confirmed an up-to-date local log and verified account identity. It recorded hashes of the canonical local export, remote schema 2 manifest and referenced XML snapshot, plus the remote file IDs and a hash of the stable account ID. Tokens and account labels were not printed or saved to evidence files.

Google's actual refresh token was submitted in a form POST to `https://oauth2.googleapis.com/revoke`; the response was HTTP 200. Google's documented revocation applies to the user's grant for the project, including its access and refresh tokens. Dropbox's actual access token was used for `POST https://api.dropboxapi.com/2/auth/token/revoke`, also returning HTTP 200. Dropbox documents that this revokes the access token, its corresponding refresh token, and the other access tokens associated with that refresh token. See [Google's native-app revocation documentation](https://developers.google.com/identity/protocols/oauth2/native-app#tokenrevoke) and [Dropbox's official auth API specification](https://github.com/dropbox/dropbox-api-spec/blob/main/auth.stone).

These calls revoked real grants. They did not substitute synthetic invalid tokens or call Neo's local Disconnect action. Provider account-settings screens were not exercised, and no cloud file was uploaded or deleted during this pass.

## Results

Both providers passed the same five native stages, followed by an independent read-only cloud comparison:

| Check | Google Drive | Dropbox |
| --- | --- | --- |
| Preflight: verified identity and up-to-date disposable log | Passed | Passed |
| Revoked access token rejected; retry rejected the refresh grant | Passed | Passed |
| Credentials, identity, baseline, primary/backup assignments and last-sync state cleared | Passed | Passed |
| Restart stayed disconnected with the local dive intact | Passed | Passed |
| Owner completed new browser consent; explicit cloud download restored the baseline | Passed | Passed |
| Connected restart verified the account and returned up to date | Passed | Passed |
| Same stable account, manifest ID/content and XML snapshot ID/content after reconnect | Passed | Passed |
| Canonical local XML byte-identical throughout all five native stages | Passed | Passed |

The first sync after revocation followed the existing HTTP 401 path and offered another sync to renew authorization. The harness explicitly retried; the revoked refresh grant then caused local cleanup and guidance to connect again. This is a two-attempt recovery path, not a claim that the first failed request immediately disconnected the provider.

Windows Credential Manager checks confirmed both the provider token entry and sync-state entry were absent after revocation and after the disconnected restart. New consent then saved a fresh connection, and downloading the existing cloud copy established its baseline. Each original disposable profile ended connected to its original provider as primary, with automatic sync off. All test processes exited. The local dive and remote files remained unchanged; unrelated local files and ordinary user logs were preserved.

Evidence remains local and untracked under `work/provider-revocation-fcfc5f-google/` and `work/provider-revocation-fcfc5f-dropbox/`. Each contains stage logs, exports, exit codes and a final `state.json` with `passed: true`. The helper is `work/test-live-provider-revocation.py`; revocation is guarded against blind repeat requests. Its native stages reject unexpected uploads.

## Validation limits and next gates

The tested binary already passed [Windows build, packaged launch, Clean/Modern and lifecycle checks](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38925358118), [desktop and mobile-target cloud regression](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38925358189), and [planner regression](https://github.com/Three-Cats-LSP/subsurface/actions/runs/38925358123). This follow-up adds ten live native Windows stages and the real revocation/reconnection evidence; it does not represent a new binary or a repeat of every CI check.

Provider account-settings UI, switching between two different live accounts, Google foreground sync, physical network loss/stalled transfers beyond controlled proxy tests, snapshot retention and public OAuth provisioning remain separate work. Physical Android validation is deferred at the owner's request. Automatic sync was disabled throughout this pass; earlier automatic-sync evidence remains in the provider testing documents. Milestone 13 is not complete.

# Conditional deletion prerequisites

Windows live-provider probes, 2026-10-27. These tests created their own uniquely named text files and deleted only the explicit object IDs returned by those creations. No existing schema 1/2 object, history namespace or local credential was changed. This is adapter evidence for [future retention](NEO_CLOUD_RETENTION.md), not an enabled application deletion feature.

## Observations

| Provider operation | Stale revision | Current revision | Repeat on the same deleted ID |
| --- | --- | --- | --- |
| Google Drive v2 `files.delete`, `If-Match` with the v2 File ETag | HTTP 412; updated bytes and revision retained | HTTP 204 | HTTP 404 |
| Google Drive v3 `files.delete`, the same stale precondition | HTTP 204; disposable updated file deleted | Not a conditional guarantee | Do not use this operation for guarded retention |
| Dropbox `files/delete_v2`, exact file ID and `parent_rev` | HTTP 409; updated bytes and revision retained | HTTP 200 | HTTP 409/not found |

These are measured outcomes on the existing authorized disposable accounts. They must not be generalized to a documented guarantee that every Drive endpoint enforces `If-Match`. The official references describe [Drive v2 deletion](https://developers.google.com/workspace/drive/api/reference/rest/v2/files/delete), [v3 deletion](https://developers.google.com/workspace/drive/api/reference/rest/v3/files/delete), and [Dropbox's explicit `parent_rev` parameter](https://docs.dropboxapi.com/dropbox-api/api-reference/user-endpoints/files/delete-v-2). Google App Data cannot be trashed, as described in [its constraints](https://developers.google.com/workspace/drive/api/guides/appdata#constraints_on_the_application_data_folder); deletion there is permanent.

Two requests against one exact newly created Google ID returned 204 and 404; a further exact-ID retry returned 404. Dropbox returned 200 and 429 under concurrent write pressure, followed by 409/not found on an exact-ID retry. The Dropbox result demonstrates a throttling path, not that both competing requests completed successfully. A future adapter must retain the deletion fence on 429 and must not widen the target or revive readers.

Both providers also received a deletion request whose acknowledgement was deliberately never read. The driver allowed processing, closed the connection and verified the exact ID was absent, with the account identity independently reverified. This exercises committed-but-unacknowledged recovery at the API boundary. It does not test a native collector, process restart or retirement state machine; those remain separate gates. An earlier Dropbox attempt closed the connection sooner, observed the file still present and recovered by deleting the same exact ID with its current revision.

## Scope and evidence

The driver verifies the provider account, inventories all pages before and after, checks each created object's identity/name/type and revision, validates the updated body after stale rejection, and compares the pre-existing inventory and original credential/baseline bytes. It has no prefix sweep or folder deletion. Cleanup uses only this invocation's explicit creation registry.

Eleven disposable files were created and removed across all attempts: five Google and six Dropbox. One Google fixture attempt stopped before the deletion test because the server identified its `.txt` file as `text/plain`, while the driver expected `application/octet-stream`. The sole created ID was verified and conditionally removed; the corrected driver accepts those two file MIME types and still excludes folders. That failed fixture and its exact-ID cleanup are retained as evidence.

Local untracked evidence:

- `work/conditional-delete-google-20261027/`: initial fixture and verified cleanup.
- `work/conditional-delete-google-20261027-retry/result.json`: both Google API versions, concurrent requests and unread acknowledgement.
- `work/conditional-delete-dropbox-20261027/result.json`: first completed Dropbox sequence.
- `work/conditional-delete-dropbox-20261027-retry/result.json`: completed sequence with committed unread acknowledgement and exact-ID retry after throttling.
- Driver: `work/probe-cloud-conditional-delete.py`. Credentials and provider response bodies are never printed or committed.

Native retirement/deletion fences, provider-backed age evidence, pins/recovery roots, production migration, multiple-active-root resolution and collector interruption/restart tests remain required. Keep all existing logs protected and application deletion disabled.

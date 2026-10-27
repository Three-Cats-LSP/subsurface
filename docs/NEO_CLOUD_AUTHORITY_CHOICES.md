# Production workspace authority: explicit selection

2026-10-28 JST. **Owner decision: explicit workspace selection, with current private storage permissions and no new service.** Windows source archival and its preceding safeguards are validated. New-device onboarding will offer an explicit create/join choice; folder listings will not elect authority.

## Provider findings

The Google `appDataFolder` alias and physical folder ID both support metadata reads with a v2 File ETag. v3 reports `canEdit: true`, but actual v2 File PATCH requests to add a new private property returned **403 forbidden** through both names under Neo's current App Data authorization. Dedicated property insert with a File ETag also returned 403; `If-None-Match: *` on insert returned 412 even though that new property key was absent. No canonical authority key was written. A final read confirmed the original folder property map and absence of every probe key.

An isolated newly created file proved that File-ETag conditional property updates preserve another property and reject a stale writer with 412. It also proved conditional null PATCH removal (stale 412/current 200). In contrast, the dedicated v2 property DELETE ignored its stale property ETag and removed only this probe's own field; it must not be used as a conditional authority operation. Failed assertions and diagnostic traces remain in `work/google-authority-property-20261028/`; the original failed result is not relabeled as a pass. One new fixture file is retained. This is provider contract evidence, not an application regression or a proof that broader OAuth permissions would solve the folder restriction.

Official references: [App Data constraints](https://developers.google.com/workspace/drive/api/guides/appdata), [custom properties](https://developers.google.com/workspace/drive/api/guides/properties), [v2 property resource](https://developers.google.com/workspace/drive/api/reference/rest/v2/properties). Capability flags alone do not establish the required conditional-write contract; the live result above takes precedence over an assumed writable singleton.

## Reviewable product paths

| Path | New-device behavior | Safety and scope |
| --- | --- | --- |
| **Explicit workspace selection (recommended)** | Offer existing workspaces for explicit selection, or an explicit create action. Persist/share a provider-bound workspace reference for reliable joining. | Keep current private storage authorization and avoid a new service. Define a workspace by its immutable physical root identity plus fresh epoch, rather than a filename. Two concurrent creators make distinct protected workspaces; never silently merge them. Existing-data selection and local replacement still need confirmation. |
| **Coordination service** | A service atomically binds one root to a verified provider/account for automatic discovery. | Requires hosting, identity verification, privacy/security design and an availability contract. It should store authority references, not dive XML or refresh tokens. Do not build/deploy a service or broaden OAuth scopes without that product choice. |
| **Defer production history migration** | Retain the current validated schema 2 sync and development history workflow. | Keep all originals and cleanup disabled while independent Windows retention/reliability work continues. Global production authority and automatic migration remain open. |

The first path replaces “one automatically discovered root per account” with “one immutable authority per explicitly selected workspace.” A client must never recreate or replace a missing established root. New writers and payload names must be bound to that root/epoch, and copying a branch must allocate fresh identities. Older schema 1/2 and development history objects remain protected. Listing workspaces is discovery for user selection, not deletion permission or a transaction.

Before any cleanup, implement and validate root ownership, writer/read/retirement fences, conservative time evidence and migration/rollback under the chosen model. No path authorizes deletion of existing schema 1/2 objects. Dropbox's unique-path behavior can support its own singleton, but the cross-provider product behavior should be chosen deliberately rather than silently introducing a Google-only backend or broader Google permissions.

The owner selected the first path in this chat. Implement provider-verified create/join and durable workspace binding under that choice. Do not request broader Google scopes or introduce a coordination service. Physical Android and two-live-account Dropbox gates remain deferred.

The first implementation adds [physical-root references and atomic local selection](NEO_CLOUD_WORKSPACES.md). These are not active-root verification or application onboarding yet; ordinary synchronization is unchanged.

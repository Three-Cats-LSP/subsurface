# Milestone 13: completion plan

Status: 28 October 2026. Product targets are Windows and Android. The detailed
[continuation checkpoint](NEO_AUTONOMOUS_PROGRESS.md) records source revisions,
live runs, failed experiments and preservation evidence.

The owner has deferred Dropbox sync to a possible future version. Product
choices, new acceptance runs and production access target Google Drive only;
retain existing Dropbox credentials, cloud data and implementation.

The current workspace implementation, including save, restore, interrupted
recovery and automatic sync, remains development-gated. Windows tests on Google
Drive and Dropbox used two profiles on one account per provider. The original
Google ready-state refusal was safely recovered but its exact cause is not
established. Do not call the milestone complete or remove the gate on that basis.

## 1. Resolve concurrent changes in the app

Deliver a simple shared Clean/Modern pop-up with two choices: **Keep this
device** and **Keep cloud**, plus cancellation. Each choice must perform a real,
guarded operation. Review must bind the current account, exact selected
workspace, local contents, accepted legacy observation, binding and remote
head. If any changes, require a new review. Never silently pick a winner.

Preserve and verify both sides as independently recoverable workspace versions
before replacing local data or advancing its binding. If choosing the local
side, publish it conditionally from the reviewed remote head, retaining the
remote version. If choosing the remote side, first protect the local version,
then explicitly restore the selected remote version through the existing
durable save and recovery flow. The owner has removed the Combine option;
neither choice silently merges the versions. Journal the confirmation and recovery steps before
mutation; resume the same operation after interruption. A changed legacy
source must be copied and explicitly accepted under equivalent guards.

Acceptance: Google-focused manager tests and actual Windows two-profile
cases for both choices, cancellation, changed local/remote state, failed save,
lost acknowledgement, termination/restart and preservation. The interface
must show what is kept and require confirmation.

## 2. Make history sustainable

Define and implement a versioned transition for bounded workspace history and
local recovery records. Preserve pending operations, active read reservations,
selected restore versions and older unsupported data. Handle offline devices
and concurrent writes without treating age or an unknown filename as deletion
permission. Avoid allowing routine use to stop permanently at the existing
512-ticket/reservation or 1 MiB journal limits.

Acceptance: native boundary, rollback, restart, offline and concurrent-device
tests, plus Windows provider tests. No deletion of existing schema 1/2 objects
and no general cleanup until separately proven safe.

## 3. Sync Neo metadata

Add a versioned payload and migration/merge policy for equipment, collections
and planner presets alongside canonical dive XML. Preserve unknown fields and
legacy clients' data; make conflicting edits reviewable instead of silently
discarding a side. Keep portable backup compatibility.

Acceptance: round-trip, old-format migration, interrupted upload/recovery and
concurrent edit tests on Windows for Google Drive; document what remains
unsupported before exposing the feature.

## 4. Validate Windows and prepare production access

Run targeted tests during development, then one full native regression and one
matching Windows package/launch/Clean/Modern validation for the integrated
candidate. Use isolated Google profiles and namespaces. Cover real
app concurrent writers, interrupted and slow transfers, offline/reconnect,
account changes and preservation. Investigate the original Google ready-state
refusal if reproduced; keep its failed evidence.

Complete provider production OAuth setup, release credential provisioning,
privacy policy and terms review before public distribution. Google
approval may require the owner's account access and provider review. Do not
broaden permissions or publish without the appropriate authorization.

## 5. Close the Android gate

Build and install the candidate on a physical Android device, then test
onboarding, credentials, workspace join, sync, conflicts, restore, background
interruption and Windows-to-Android round trips. Physical Android is currently
unavailable and remains an explicit deferred gate.
Host-built mobile tests do not satisfy the device gate.

## Delivery rule

Each item should produce a reviewable app behavior and its focused tests before
more protocol layers are added. Preserve original logs, credentials, unrelated
local files and every existing schema 1/2 object. Use current private storage
permissions and no coordination service. Keep the work on modern-ui; do not
publish a release as part of this plan. Milestone 13 closes only after all
applicable Windows, Android, provider and release gates pass.

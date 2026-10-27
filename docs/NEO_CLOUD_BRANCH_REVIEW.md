# Read-only history branch review

Implemented 2026-10-27 behind the existing history development gate; native validation of this increment is pending. Clean and Modern share **Review branches** in Cloud & Sync. It reviews only the exact configured epoch after fresh provider account verification, with automatic sync off and no provider assignments.

Google discovery collects every page for the exact root name without choosing, renaming or binding any candidate. IDs must be unique and valid, names exact, and MIME types ordinary JSON/text/binary files. Incomplete searches, repeated or invalid cursors, unsupported entries and more than sixteen roots stop the review. Dropbox reads its unique root by name and then verifies its exact identity/revision. All bodies must validate against the expected provider, account and epoch.

Every discovered root is read twice using exact object identity and revision; Google bodies are bracketed by metadata reads. Google repeats membership discovery after those reads. A changed body, revision or membership discards the result. This is a conservative read-only observation, not a transaction, authority election, deletion plan or guarantee against later changes. No XML payload is read.

The result reports each branch's confirmed-version count, pending-upload count and active-restore count. It separately identifies agreement with this device's saved recovery record, a missing known root or a rollback. Review never writes or replaces that checkpoint, even on a fresh profile, and never applies a dive log. Cancellation and failures publish no partial report; a new history operation clears the previous report.

## Resolution still required

Two active roots remain blocked for ordinary history operations. Reviewing them does not make either authoritative. The next protocol increment must preserve every source history, pending ticket and reservation before copying or redirecting a branch. A newer root format must fence old writers through conditional state transitions, persist recovery intent before changing another root, and retain exact source identities and handoff records across interruption. A client bound to an active source needs a verified handoff before rebinding; the pristine-alias exception cannot be widened to accept active replacements.

Migration and rollback must use fresh namespace identities and retain all schema 1/2 files. Production discovery, guarded automatic-sync migration, protected branches, provider-backed age evidence, pins and retirement/deletion fences remain separate gates. No deletion operation is added here.

Regression scenarios cover both providers, multiple active Google roots, pristine companions, pending uploads/reservations, existing and absent checkpoints, rollback/missing-root reporting, corrupt/cross-account/cross-epoch roots, revision and membership changes, pagination and invalid cursors/metadata, safety bounds, local edits and cancellation. Exact Windows application/CI/live evidence must be recorded after validation, rather than inferred from an older binary.

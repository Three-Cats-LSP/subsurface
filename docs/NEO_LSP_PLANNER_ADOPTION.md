# Reuse of our LSP+ planner

The owner's 28 October instruction is to use our LSP+ planner and the software,
algorithms and validation material developed previously. This supersedes the
earlier Subsurface-only planner boundary. Windows and Android remain the product
targets; this does not introduce a browser or Linux product.

## Pinned software and earlier evidence

The earlier read-only reference checkout was LSP+ 3.1.5 at `30a04ed`. The upstream
main branch was checked and the adoption source is **LSP D-Planner+ 3.2.0**,
[`05fef18bade6354cebcc60df01f83e0ac4cc3b27`](https://github.com/Three-Cats-LSP/LSP_D-planner-plus/tree/05fef18bade6354cebcc60df01f83e0ac4cc3b27).
The original checkout is untouched. A separate sparse source checkout is retained
at `work/lsp-planner-adoption-05fef18-20261028/`.

`third-party/lsp-planner/SOURCE.json` records exact source hashes. Four upstream
files are preserved byte-for-byte: ZHL bundle, VPM bundle, schedule runner and
headless adapter. The native host calls their actual calculations, input guards,
CCR/pSCR physics, repetitive loading and exposure integration. Qt-incompatible
syntax is translated reproducibly with the locked Babel toolchain; the
calculations are not rewritten. The six full upstream presets are retained,
including water vapour, helium convention, transit and rounding settings omitted
by the old native mappings. Upstream declares MIT licensing in its README.

The same pinned repository supplies the previous material, including:

- `Knowledge Base/MultiDeco_Verified_Findings.md` and
  `MultiDeco_Verified_Findings_2_Claude.md`, plus the full engine and shallow
  gradient analyses;
- `Abysner_Analysis.md`, `Abysner_GUE_Crosscheck_Addendum.md`,
  `GUE_DecPlanner_Analysis.md`, `DiveKit_Engine_Knowledge_Base.md` and
  `Subsurface_Engine_Analysis.md` in that Knowledge Base;
- `docs/CCR_ENGINE_DIFFERENTIAL_TEST_PLAN.md` and
  `docs/CCR_LSP_SUSPECT_AUDIT.md`;
- 21 CCR differential fixtures, captured comparison goldens, their provenance,
  expected-difference ledger and original comparison helper, now retained under
  `tests/data/lsp-planner/`.

Use the later verified findings and actual production code where older research
notes disagree. Preserve comparator limitations: the Abysner/Subsurface goldens
are open-reference implementations; MultiDeco/DiveKit captures cover C1–C3;
the existing precision and multilevel/repetitive comparator limitations do not
become new third-party parity claims.

One input-contract detail must be retained during UI integration: the imported
CCR fixtures label `timeConvention` as `at-depth`, but their original
`levelsFromFixture` passes that value unchanged to LSP's descent-inclusive
first-level runtime. The capture intentionally reproduces that existing call;
it does not validate the label or silently add another descent. Neo's runtime
waypoints and LSP continuation durations need explicit conversion tests and
clear display labels before enabling the public path.

## Current executable evidence

`tools/lsp-planner/capture-reference.cjs` calls the **original**, untranslated
production engine in Node and refuses to overwrite a reference. The reviewed
38-case capture is `tests/data/neo-lsp-regression.json` (SHA-256
`0c55a9f278ae57840c5b740f4035fcdc61e9b7fd766882dbf0f7571e460ea8fd`). It includes
all 21 existing CCR cases plus OC air/nitrox, multiple gases/lost oxygen,
multilevel, altitude/freshwater, alternate helium convention, all five LSP VPM
variants, pSCR and CCR in both engine families.

On this Windows machine the translated runtime matched **all 38** original
results in a real Qt **6.10.3 QJSEngine**, comparing every output field with
numeric tolerance `1e-7`. Evidence:
`work/lsp-native-reference-05fef18-qt6103-20261028.json`. The first probe exposed
unsupported object-spread syntax and absent `globalThis`; the syntax translation
and isolated global alias fix these without changing calculations. An initial
comparison also caught a different invalid-gas error structure; the host now
uses LSP's original validation order. The failed probe outputs remain in chat.

`qt-models/lspplanner.*` provides an isolated, fresh engine per request, explicit
environment/settings, no native objects/network/filesystem exposed to scripts,
resource hashes, input/output bounds and a ten-second interrupt deadline. The
model exposes `calculateLspPlan`, `lspPlannerIdentity` and the complete presets.
The C++ regression adds the 38 reference cases, 13 refusal cases, preset/identity
checks and cross-calculation state isolation. Standalone Windows Qt 6.8.3 and
hosted shared-code builds are required; the local Python/Qt probe does **not**
claim that the C++ application build or package passed.

## Remaining integrated acceptance

1. Pass the standalone C++ comparison on Windows Qt 6.8.3 and the shared-code
   host, then the full native application suites. Repair failures on the same
   pinned source without changing reviewed expectations to match failures.
2. Connect shared Clean/Modern plan, NDL and contingency actions to the LSP
   boundary. Translate Neo runtime waypoints explicitly into the existing LSP
   level-time contract, including continuation levels, gas roles, circuit and
   setpoints. Load actual sampled prior dives into LSP state; never carry stale
   engine state between requests or mix Subsurface projections into an LSP plan.
3. Use the six full LSP presets and expose their actual advanced parameters.
   Preserve old native-mapping presets under their original engine/version;
   personal presets retain complete resolved settings, not only a name/GF pair.
4. Adapt LSP schedule, gas/exposure results and chart fields to Neo. Preserve
   cylinder/reserve and unavailable-gas refusal behavior. Mark unavailable
   analysis fields explicitly instead of inventing native GF/ceiling values.
5. Save and reopen actual LSP-generated profiles through existing local dive
   persistence and backup, carrying engine ID, source version, effective input
   and schedule. Text/PDF/slate and contingency exports must match that same
   calculation. Reopening old Subsurface plans must retain their old identity.
6. Validate actual Windows Clean/Modern edits, both units, preset save/load,
   repetitive loading, contingencies, refused save and exports in a matching
   package. Continue remaining cloud/metadata work independently. Physical
   Android and external compatibility claims retain their documented gates.

The app's existing public planner still calls the Subsurface path until steps
2–6 are implemented and validated. This checkpoint is concrete engine reuse,
not a claim that the visible planner switch or the Windows release is complete.

## CI checkpoint

**Native adoption gate passed:** run **40708771596**, source `151e53b2f`, passes
source/fixture hashes, reproducible translation, all 38 original-Node references
and native C++ testing on both Windows Qt **6.8.3** and the shared-code host. The
suite has **55 checks**. The functional fix at `aa0f20f70` passes plain JSON into
JavaScript, avoiding QVariant sequence conversion differences across Qt versions.
No reference or LSP mathematics was changed to pass these tests.

Earlier failures are retained: **40705358571** had an empty Windows CTest log;
**40706143649 / 40706743408** failed in GUI-plugin deployment; **40707615383**
ran Qt 6.8.3 and exposed 16 passes / 39 request refusals; **40708302494** caught
a PowerShell version-argument quoting error before running tests. The earlier
incomplete-cache diagnosis was disproven by explicit dependency checks. The
successful headless runner deploys its four actual Qt libraries directly.

The original integrated app source `ba0db8dc2` passed Windows launch,
Clean/Modern, lifecycle and package run **40705358568**, plus Android compile
**40705358554**. Those builds predate the JSON fix and do not exercise a public
LSP planner flow. Full native app **40707131405** remains pending on 6c97 and
also predates the JSON fix. Preserve these source-specific results; final
matching-source app/package validation follows the actual UI/save/export work.

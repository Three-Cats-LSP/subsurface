# Neo planner external-reference audit

## Audited source

The external comparison source is [Three-Cats-LSP/LSP_D-planner-plus](https://github.com/Three-Cats-LSP/LSP_D-planner-plus), revision `30a04edb6ed258c3b1c4ae37b7a326d0ee934dc0` (2026-08-12 audit). It is MIT licensed and provides CCR differential fixtures and `subsurface`-labelled comparison goldens for CCR baseline, altitude, bailout, lost-gas, repetitive, setpoint, GF and precision scenarios.

Neo deliberately does not import its JavaScript decompression engines. The Neo planner continues to call Subsurface's native `plan()` and `create_plot_info_new()` paths.

## Adoption boundary

The first audited `CCR-C1` fixture requires assumptions that are not all exposed by the current Subsurface planner API used by Neo:

- water-vapour pressure;
- stop step and whole-minute stop rounding;
- a four-rate ascent profile;
- an explicit reference engine/preset identity.

Its published `subsurface` golden is an open-reference result, not an authoritative Subsurface test fixture at this repository revision. Copying its schedule or tolerance numbers into Neo tests would therefore make an unverified compatibility claim.

## Current Neo coverage

`TestDivePlannerModel::testNeoPlanResultContract` exercises the public Neo planner boundary for:

- OC, pSCR, CCR diluent/setpoint and VPM-B;
- trimix and reduced surface pressure;
- custom water density;
- GF/surface-GF, ceiling, NDL, TTS, pO2, tissue and CNS projections;
- gas analysis, contingency variations, and unavailable-gas rejection.

`TestDivePlannerModel::testNeoPlannerNativeRegression` adds representative
native-engine checks for no-decompression and decompression profiles, GF
conservatism, gas/OTU analysis and VPM-B conservatism. These deliberately test
observable safety contracts (valid result, monotonic depth/runtime and expected
relative conservatism) instead of importing an external engine's schedule.

The dedicated `Neo planner regression` workflow runs the desktop and mobile
native test suites on `modern-ui`; Android Qt 6 and Windows MSVC also compile
the same planner and QML boundary.

## Windows and Android scope

Windows and Android are the current product targets. Earlier WebAssembly test
page ideas are outside the current delivery plan. Native Windows regression
fixtures and the shared Android planner must continue to use Subsurface's own
`plan()` and `create_plot_info_new()` pipeline.

The in-progress native fixture manifest records Windows x64 results captured
from the matching Subsurface revision, with explicit runtime, stop and OTU
tolerances. These are regression baselines for that engine, not independent
medical validation or parity with an external engine. They remain unvalidated
until the matching native suites pass on the changed source.

## Required before compatibility labels or numerical parity claims

1. Version a Subsurface-compatible fixture manifest that records every effective native preference.
2. Map each external scenario only where all calculation assumptions can be represented by that manifest.
3. Capture expected schedules from the matching Subsurface revision and define explicit per-field tolerances.
4. Keep incompatibilities as documented differences; do not relabel a close result as compatible.
5. Add the resulting fixtures to the native test suite before exposing LSP, MultiDeco, or other compatibility-profile names in Neo.

This audit is a validation input, not a decompression-engine implementation or an interoperability claim.

The audited preset values and their field-by-field native representability are
recorded in [Neo planner compatibility-profile matrix](NEO_PLANNER_COMPATIBILITY_MATRIX.md).

## Windows-first coverage expansion, 28 October 2026

The fixture candidate now contains 18 data-driven scenarios, retaining the five
earlier expected schedules and adding nitrox, CCR/trimix setpoint, pSCR,
multilevel, three-gas/lost-oxygen, freshwater/altitude, recreational NDL refusal,
low reserve, exhausted gas and repetitive tissue initialization. A hashed sampled
prior dive ends ten minutes before the repetitive target; its captured runtime
is 3200 seconds versus 2060 for the otherwise identical fresh-tissue profile.
The manifest fixes effective native preferences and numerical tolerances, and
the native runner isolates global dive/catalog state for each row.

`scripts/capture-neo-planner-reference.py` checks the Windows executable against
its verified-package metadata, archives the matching source QML and uses a fresh
credential-free profile. It records actual outputs and effective preferences;
it never changes expected values. Final Windows evidence is retained under
`work/planner-native-reference-a92c17bf0-20261028-final-18/`. Earlier exploratory
captures remain retained, including a corrected capture-helper pO2 unit error
and a generated-primer attempt that did not establish the final sampled-profile
case. Do not use them as additional accepted references.

The captured calculation source is a92; new C++ fixture assertions and the
surface-interval output still require their own native regression. Until that
passes this expansion is a candidate, not a completed release gate. These
references compare this engine with itself. Independent cross-engine parity,
unrepresentable compatibility presets and physical Android remain unproven.

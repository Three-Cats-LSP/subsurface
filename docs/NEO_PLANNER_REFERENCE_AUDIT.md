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

## Future Neo Web test pages

The planned WebAssembly build must compile and call the same Subsurface
`plan()` and `create_plot_info_new()` pipeline as Neo native. The browser test
pages will be thin scenario runners over that module; JavaScript must not
implement, approximate or silently replace decompression mathematics.

The eventual public URLs under `https://threecats-lsp.com/subsurface-neo/`
will provide the Core, Verify, Extended, Massive, Mobile Massive, pSCR
OTU/CNS and CCR differential groups as separate pages that open in new tabs.
Their source scenarios may be adapted from the LSP suites where the input
assumptions are representable, but their expected results will be captured
from the matching Subsurface revision and versioned with documented
tolerances. Until the WebAssembly planner module exists, no static page should
claim to execute Neo's engine.

## Required before compatibility labels or numerical parity claims

1. Version a Subsurface-compatible fixture manifest that records every effective native preference.
2. Map each external scenario only where all calculation assumptions can be represented by that manifest.
3. Capture expected schedules from the matching Subsurface revision and define explicit per-field tolerances.
4. Keep incompatibilities as documented differences; do not relabel a close result as compatible.
5. Add the resulting fixtures to the native test suite before exposing LSP, MultiDeco, or other compatibility-profile names in Neo.

This audit is a validation input, not a decompression-engine implementation or an interoperability claim.

The audited preset values and their field-by-field native representability are
recorded in [Neo planner compatibility-profile matrix](NEO_PLANNER_COMPATIBILITY_MATRIX.md).

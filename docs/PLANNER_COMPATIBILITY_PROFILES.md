# Subsurface Neo — Planner Compatibility Profiles

## Purpose

Subsurface Neo should let divers moving from established decompression planners continue planning with the assumptions and schedule behavior they already know.

A compatibility profile is **not** a marketing preset and is not merely a Gradient Factor preset. It is a versioned bundle of calculation and schedule-generation parameters that can materially affect the resulting dive schedule.

Initial compatibility targets should include, where the source behavior can be verified:

- **Subsurface** — preserve the current upstream Subsurface planner behavior and defaults.
- **MultiDeco** — reproduce the relevant MultiDeco-style calculation/profile assumptions already studied and represented in LSP D-Planner+ where technically and legally appropriate.
- **LSP D-Planner+** — reproduce the corresponding LSP+ planner assumptions and schedule behavior.
- Other established planners may be added only after their relevant behavior is documented and regression-tested.
- **Custom** — fully user-controlled advanced settings.

The purpose is migration continuity: a MultiDeco user should be able to select the MultiDeco compatibility profile in Neo and obtain a schedule shaped by the same verified assumptions they are accustomed to, while a Subsurface user should be able to retain Subsurface-style behavior.

## Profile contents

A compatibility profile may control parameters such as, where supported by the selected decompression model and Neo calculation path:

- decompression algorithm / model variant,
- Gradient Factors or VPM conservatism settings,
- water-vapor pressure convention,
- helium half-time / coefficient convention where applicable,
- water-density / depth-to-pressure convention,
- altitude and surface-pressure handling,
- acclimatization behavior,
- descent rate,
- deep ascent rate,
- deco ascent rate,
- final/surface ascent rate,
- stop spacing / deco-step behavior,
- last-stop depth,
- gas-switch behavior,
- PPO2 / MOD / MinOD assumptions where relevant,
- narcotic-gas assumptions where relevant,
- rounding and schedule presentation rules that materially affect generated stop/run times,
- model-specific advanced parameters.

This list is intentionally broader than the current Subsurface planner preferences. Exact fields must be determined by auditing Subsurface, LSP D-Planner+, reference planners and available validation material.

## Required UX

Planner settings should expose a top-level **Compatibility Profile** selector before advanced individual controls.

Example presentation:

- Subsurface
- MultiDeco
- LSP D-Planner+
- Custom

Selecting a profile must:

1. Apply the complete verified parameter bundle atomically.
2. Clearly show the active profile beside the generated plan.
3. Populate Advanced Settings with the actual resulting parameter values.
4. Allow the user to inspect every parameter changed by the profile.
5. Switch the profile state to **Custom (modified from X)** when the user changes a profile-controlled value.
6. Offer an explicit **Reset to profile defaults** action.
7. Preserve user-created custom profiles/templates separately from built-in compatibility profiles.

The UI must never imply that two planners are mathematically identical merely because the same algorithm name or GF values are selected.

## Versioning

Built-in compatibility profiles must be versioned because external planner behavior can change across releases.

Conceptually:

```text
profile_id: multideco
profile_version: 1
source_reference: verified MultiDeco/LSP+ compatibility study
parameters:
  ...
```

A saved Neo plan should retain enough information to identify the exact compatibility-profile version and effective calculation parameters that generated it.

If a future Neo release changes a built-in compatibility profile, an old saved plan must remain interpretable and reproducible rather than silently inheriting new defaults.

## Parameter manifest

Implement profiles as data, not as scattered UI conditionals.

Conceptual structure:

```text
PlannerCompatibilityProfile
  id
  displayName
  version
  description
  algorithmApplicability
  parameterManifest
  sourceNotes
  validationFixtureSet
```

The planner engine should consume the resolved parameter manifest. QML/UI should not contain hidden compatibility calculations.

## Validation

A compatibility claim requires numerical regression evidence.

For each built-in profile, maintain reference fixtures covering appropriate combinations such as:

- simple square air dive,
- nitrox decompression dive,
- trimix dive,
- multilevel profile,
- multiple deco gases,
- CCR where applicable,
- pSCR where applicable,
- altitude case,
- repetitive dive,
- long/deep decompression case,
- gas-switch boundary cases,
- different last-stop / stop-step cases.

Compare at minimum:

- first stop depth,
- individual stop depths and times,
- run times,
- total ascent/decompression time,
- TTS,
- gas switches,
- tissue/GF values where comparable,
- gas consumption where the source planner defines equivalent assumptions.

Differences must be documented. Neo should never label a profile as compatible merely because the result is "close" without defining the allowed tolerance and explaining unavoidable implementation differences.

## LSP D-Planner+ reference

LSP+ is particularly useful because it already exposes and documents several compatibility-sensitive advanced settings, including environment/depth-pressure conventions and model settings derived from or compared with established planners.

Examples already identified in the LSP+ codebase include:

- a water-vapor default explicitly annotated as the LSP/MultiDeco value,
- salt-water pressure convention annotated as matching MultiDeco/DiveKit/ApexDeco,
- fresh-water and EN13319 pressure conventions,
- configurable helium half-time mode,
- altitude/acclimatization controls,
- decompression algorithm-specific advanced settings.

During Neo implementation, audit the current LSP+ Advanced Settings UI and source completely and transfer the verified compatibility manifests and regression cases rather than copying only labels.

## Subsurface compatibility

The **Subsurface** profile must be derived from Neo's actual upstream Subsurface planner preferences and engine behavior at the compatibility baseline commit.

Where possible, the compatibility layer should translate a profile into existing upstream planner preferences rather than fork or duplicate the decompression engine.

The profile should therefore serve two purposes:

1. preserve familiar behavior for existing Subsurface users;
2. provide an explicit baseline against which MultiDeco/LSP+/Custom profile differences can be displayed and tested.

## Plan and export metadata

Every generated advanced plan and planner export should include the effective calculation identity, for example:

```text
Algorithm: Bühlmann ZHL-16C + GF
GF: 30/85
Compatibility profile: MultiDeco
Profile version: 1
Water/depth convention: ...
Water vapor: ...
He half-time convention: ...
Stop step: ...
Last stop: ...
Ascent rates: ...
```

Only parameters relevant to that algorithm/profile need to be shown in the compact view, but the full parameter manifest should be available in plan details/export metadata.

## Safety and product language

Use language such as **MultiDeco compatibility profile** or **Subsurface compatibility profile**, not claims that Neo *is* MultiDeco or that schedules are guaranteed identical across all versions and configurations.

Compatibility must be tied to documented source versions/settings and regression fixtures.

## Implementation priority

This feature belongs in Neo Milestone 8 — Advanced Dive Planner & Decompression Lab.

Implementation order:

1. Inventory all relevant current Subsurface planner preferences and calculation constants.
2. Inventory every LSP+ Advanced Settings field and its effective engine parameter.
3. Extract the existing LSP+ planner-comparison/compatibility values and validation material.
4. Define the first versioned Subsurface and LSP+ profile manifests.
5. Define MultiDeco-compatible manifest only for settings we can verify.
6. Add reference schedule fixtures before exposing a compatibility label publicly.
7. Add compatibility-profile selector and Advanced Settings diff/inspection UI.
8. Persist profile ID/version plus resolved effective parameters with saved plans.
9. Include compatibility metadata in Copy/TXT/PDF/Deco Slate exports.
10. Add profile regression tests to the permanent Neo planner release gate.

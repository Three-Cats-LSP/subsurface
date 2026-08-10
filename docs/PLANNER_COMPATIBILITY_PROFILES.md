# Subsurface Neo — Planner Compatibility Profiles

## Purpose

Subsurface Neo should let divers moving from established decompression planners continue planning with the assumptions and schedule behavior they already know.

A compatibility profile is **not** a marketing preset and is not merely a Gradient Factor preset. It is a versioned bundle of calculation and schedule-generation parameters that can materially affect the resulting dive schedule.

Initial compatibility targets should include, where the source behavior can be verified:

- **Subsurface** — preserve the current upstream Subsurface planner behavior and defaults.
- **MultiDeco** — reproduce the relevant MultiDeco-style calculation/profile assumptions already studied and represented in LSP D-Planner+ where technically and legally appropriate.
- **LSP D-Planner+ / LSP Default** — reproduce the corresponding LSP+ planner assumptions and schedule behavior.
- **Abysher** — include the LSP+ reference preset once its exact parameter manifest is audited and validated.
- **GUE DecPlanner** — include the LSP+ reference preset once its exact parameter manifest is audited and validated.
- **DiveKit** — include the LSP+ reference preset once its exact parameter manifest is audited and validated.
- Other established planners may be added only after their relevant behavior is documented and regression-tested.
- **Custom** — fully user-controlled advanced settings.

The purpose is migration continuity: a MultiDeco user should be able to select the MultiDeco compatibility profile in Neo and obtain a schedule shaped by the same verified assumptions they are accustomed to, while a Subsurface user should be able to retain Subsurface-style behavior.

## Built-in profiles are loadable presets

Neo must implement compatibility profiles as a user-facing **loadable preset library**, following the useful LSP+ Advanced Configs workflow rather than hiding them behind individual advanced controls.

The Planner should expose a Profile/Presets action from the main planner UI. Opening it presents two groups:

### App reference presets

Built-in, read-only presets supplied by Neo, initially modeled on the reference set already exposed by LSP+:

- LSP Default
- MultiDeco
- Abysher
- Subsurface
- GUE DecPlanner
- DiveKit

Only presets whose parameter manifests and validation status are known should be enabled for public release. A preset may remain marked experimental/internal while validation is incomplete.

Each preset row should show a compact summary of the settings that most strongly distinguish it, for example:

```text
MultiDeco
WV 0.0577 · MultiDeco transit · Whole-minute rounding · ...
                                                   [ Load ]

Subsurface
WV 0.0627 · Schreiner transit · Fractional stops · ...
                                                   [ Load ]
```

The summary is informational only. Pressing **Load** must atomically apply the complete versioned parameter manifest.

### My profiles

Users can save their current advanced settings as reusable personal profiles.

Required actions:

- **Save Current**
- rename a saved profile,
- load a saved profile,
- duplicate a built-in profile into a personal editable profile,
- delete a personal profile,
- export/import personal profiles where practical,
- sync/backup personal profiles as Neo metadata.

Built-in reference presets must never be overwritten by user edits.

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
- minimum-stop behavior,
- gas-switch behavior,
- transit calculation method,
- PPO2 / MOD / MinOD assumptions where relevant,
- narcotic-gas assumptions where relevant,
- whole-minute versus fractional-stop handling,
- stop-time/run-time rounding behavior,
- schedule presentation rules that materially affect generated stop/run times,
- model-specific advanced parameters.

This list is intentionally broader than the current Subsurface planner preferences. Exact fields must be determined by auditing Subsurface, LSP D-Planner+, reference planners and available validation material.

## Required UX

Planner settings should expose both:

1. a visible **Active Profile** indicator in the planner; and
2. a **Profiles / Presets** menu that opens the loadable preset library.

Selecting **Load** on a profile must:

1. Apply the complete verified parameter bundle atomically.
2. Clearly show the active profile beside the generated plan.
3. Populate Advanced Settings with the actual resulting parameter values.
4. Allow the user to inspect every parameter changed by the profile.
5. Recalculate the current plan immediately when safe/appropriate.
6. Switch the profile state to **Modified from X** when the user changes a profile-controlled value.
7. Offer an explicit **Reset to X defaults** action.
8. Preserve user-created custom profiles/templates separately from built-in compatibility profiles.

Example state:

```text
Active profile: MultiDeco
```

After changing one advanced setting:

```text
Active profile: MultiDeco — Modified
[ Reset to MultiDeco ]   [ Save as My Profile ]
```

The UI must never imply that two planners are mathematically identical merely because the same algorithm name or GF values are selected.

## Advanced Settings integration

The Advanced Settings screen remains the detailed editor for the resolved profile values. It should include every compatibility-sensitive parameter that Neo exposes, grouped by purpose rather than by source application.

Suggested groups:

- Algorithm / model
- Gradient / VPM settings
- Transit / tissue integration
- Water vapor / pressure model
- Water density
- Ascent / descent rates
- Stop generation / rounding
- Last stop / minimum stop
- Gas switching / PPO2 / MOD / MinOD
- Narcosis assumptions
- Altitude / acclimatization
- CCR / pSCR-specific options

The Profiles menu is therefore a fast way to load a complete known configuration; Advanced Settings is where experts inspect or customize it.

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

When Neo ships an updated built-in profile, the UI may show that a newer preset definition exists, but must not silently alter already-saved plans.

## Parameter manifest

Implement profiles as data, not as scattered UI conditionals.

Conceptual structure:

```text
PlannerCompatibilityProfile
  id
  displayName
  version
  description
  builtIn
  readOnly
  algorithmApplicability
  parameterManifest
  sourceNotes
  validationFixtureSet
  validationStatus
```

The planner engine should consume the resolved parameter manifest. QML/UI should not contain hidden compatibility calculations.

User profiles use the same manifest structure but have their own IDs/names and are editable.

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

LSP+ is particularly useful because it already exposes an **Advanced Configs** preset loader and documents several compatibility-sensitive advanced settings, including environment/depth-pressure conventions and model settings derived from or compared with established planners.

The current LSP+ UI demonstrates the desired user workflow:

1. open the planner menu,
2. open Advanced Configs / Profiles,
3. see the built-in app-reference presets,
4. press **Load** beside the desired planner profile,
5. optionally save the current advanced configuration as a personal preset.

Examples already identified in the LSP+ codebase include:

- a water-vapor default explicitly annotated as the LSP/MultiDeco value,
- salt-water pressure convention annotated as matching MultiDeco/DiveKit/ApexDeco,
- fresh-water and EN13319 pressure conventions,
- configurable helium half-time mode,
- altitude/acclimatization controls,
- decompression algorithm-specific advanced settings,
- different transit/rounding/stop-generation choices represented by its reference presets.

During Neo implementation, audit the current LSP+ Advanced Settings UI and source completely and transfer the verified compatibility manifests and regression cases rather than copying only labels.

## Subsurface compatibility

The **Subsurface** profile must be derived from Neo's actual upstream Subsurface planner preferences and engine behavior at the compatibility baseline commit.

Where possible, the compatibility layer should translate a profile into existing upstream planner preferences rather than fork or duplicate the decompression engine.

The profile should therefore serve two purposes:

1. preserve familiar behavior for existing Subsurface users;
2. provide an explicit baseline against which MultiDeco/LSP+/other/Custom profile differences can be displayed and tested.

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
Transit model: ...
Stop rounding: ...
Stop step: ...
Last stop: ...
Ascent rates: ...
```

Only parameters relevant to that algorithm/profile need to be shown in the compact view, but the full parameter manifest should be available in plan details/export metadata.

## Persistence, backup and sync

Built-in profiles ship with Neo and are versioned with the application.

Personal profiles should be stored as Neo metadata and included in:

- local Neo portable backups,
- Google Drive / Dropbox Neo metadata sync where safe,
- migration between desktop, Android and Web versions.

A user should be able to configure a planner profile on Windows and see the same personal profile available on Android/Web after Neo metadata synchronization.

## Safety and product language

Use language such as **MultiDeco compatibility profile** or **Subsurface compatibility profile**, not claims that Neo *is* MultiDeco or that schedules are guaranteed identical across all versions and configurations.

Compatibility must be tied to documented source versions/settings and regression fixtures.

## Implementation priority

This feature belongs in Neo Milestone 8 — Advanced Dive Planner & Decompression Lab.

Implementation order:

1. Inventory all relevant current Subsurface planner preferences and calculation constants.
2. Inventory every LSP+ Advanced Settings field and its effective engine parameter.
3. Extract the exact built-in LSP+ reference-preset manifests: LSP Default, MultiDeco, Abysher, Subsurface, GUE DecPlanner and DiveKit.
4. Extract the existing LSP+ planner-comparison/compatibility validation material.
5. Define the first versioned Subsurface and LSP Default profile manifests.
6. Define MultiDeco and other compatibility manifests only for settings we can verify.
7. Add reference schedule fixtures before exposing compatibility labels publicly.
8. Implement the Profiles / Presets loader with built-in and personal-profile sections.
9. Implement **Load**, **Save Current**, rename, duplicate and delete for personal profiles.
10. Add active-profile / modified-profile state to the planner.
11. Add Advanced Settings diff/inspection UI and Reset-to-preset behavior.
12. Persist profile ID/version plus resolved effective parameters with saved plans.
13. Include compatibility metadata in Copy/TXT/PDF/Deco Slate exports.
14. Include personal profiles in Neo backup/sync metadata.
15. Add profile regression tests to the permanent Neo planner release gate.

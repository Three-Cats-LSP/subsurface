# Neo planner compatibility-profile matrix

## Source baseline

This matrix audits `Three-Cats-LSP/LSP_D-planner-plus` revision
`30a04edb6ed258c3b1c4ae37b7a326d0ee934dc0`, specifically the six
read-only `LSP_APP_PRESETS` in `index.html` and its `_ADV_FIELDS` list in
`schedule-runner-core.js` (audited 2026-08-12).

It is an inventory, not a compatibility claim. Neo continues to use
Subsurface's native `plan()` and `create_plot_info_new()` pipeline; it does
not import the reference application's planner engine.

## Preset inventory

| External preset | GF | Descent / ascent rates (m/min) | Last stop | Bottom / deco pO2 (bar) |
|---|---:|---|---:|---:|
| LSP Default | 20/85 | 20 / 10, 3, 3 | 3 m | 1.4 / 1.6 |
| MultiDeco | 30/85 | 22 / 9, 9, 9 | 3 m | 1.4 / 1.6 |
| Abysner | 60/70 | 25 / 9, 3, 3 | 3 m | 1.4 / 1.6 |
| Subsurface | 30/70 | 20 / 9, 6, 3 | 3 m | 1.4 / 1.6 |
| GUE DecPlanner | 20/85 | 20 / 9, 3, 3 | 3 m | 1.2 / 1.2 |
| DiveKit | 50/80 | 20 / 9, 9, 3 | 6 m | 1.4 / 1.6 |

All six also specify water-vapour convention, transit method, stop rounding,
helium half-time mode, SAC assumptions, narcosis options, and minimum-stop
behaviour. Those fields are calculation-significant and cannot be omitted
from a compatibility claim.

## Native representability

| External field | Neo/Subsurface native control | Status |
|---|---|---|
| GF low/high | `planner_gflow`, `planner_gfhigh` | Representable |
| VPM-B conservatism | `vpmb_conservatism` | Representable for native VPM-B only |
| Descent, deep/mid/deco/final ascent rates | `descrate`, `ascrate75`, `ascrate50`, `ascratestops`, `ascratelast6m` | Representable |
| 3 m / 6 m last stop | `last_stop6m` | Representable for these two values |
| Bottom/deco pO2 | `bottompo2`, `decopo2` | Representable |
| SAC / reserve, gas switching, bailout | Native planner controls and cylinder/segment input | Representable |
| Water density / surface pressure | planner salinity and surface-pressure input | Representable where the scenario is otherwise equivalent |
| O2 narcotic assumption | `o2narcotic` | Representable |
| Water-vapour convention | No Neo/Subsurface planner control | Not representable |
| Transit method | No Neo/Subsurface planner control | Not representable |
| Stop rounding / fractional-stop policy | No Neo/Subsurface planner control | Not representable |
| Helium half-time convention | No Neo/Subsurface planner control | Not representable |
| Minimum-stop and shallow-gradient policy | No Neo/Subsurface planner control | Not representable |
| N2 narcosis and external CCR loop/metabolic settings | Not exposed by the current Neo planning boundary | Not representable |

## Enablement rule

No external preset name may be exposed as a Neo compatibility profile until a
versioned native manifest covers every schedule-affecting field in the target
scenario and native Subsurface reference schedules have been captured with
documented tolerances. A partially matched preset may be used by a diver as a
personal Neo preset, but must remain labelled **Custom**, not as the external
planner or profile.


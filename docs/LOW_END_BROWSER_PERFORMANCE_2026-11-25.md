# Browser performance and settings overhaul — 2026-11-25

## Profiles

| Profile | Automatic cap | Internal linear resolution | Decorative particle budget | Character mask lighting |
| --- | ---: | ---: | ---: | --- |
| Quality | 60 FPS | 100% | 1400 | Full |
| Balanced | 30 FPS | 75% | 360 | Full |
| Low End | 30 FPS | 60% | 220 | Lite |
| Auto (default) | Adapts among the above | Adapts | Adapts | Adapts |

A low-memory browser hint can start Auto at Balanced; measured sustained frame costs control subsequent transitions. Recovery is intentionally slower than downgrade. Manual frame cap, resolution, effects and lighting override the profile independently. Legacy ecoFX saves retain their explicit choice. Options remain rendered at full logical resolution for readability. All profile controls use the existing save model. No new save slot or game progression reset is required.

60% linear resolution means 36% of the original canvas pixels. 30 FPS caps submitted frames, not input sampling or the fixed 60 Hz gameplay target. Under extreme CPU saturation the existing bounded catch-up can still run below real time. Neither profile setting nor the regression tests guarantee hardware FPS.

## Implementation

- World/UI/pointer coordinates remain logical; only the backing canvas and cached lighting overlay scale. The character compositor still uses physical pixels for its intermediate masks. Settings avoid drawing scenery hidden behind the opaque panel.
- Render pacing retains deadline phase across 24–144 Hz display callbacks. Visibility changes reset the time backlog. Frame statistics count actual submitted pictures, not every callback.
- Cut character mask work to its active rectangle, avoid per-blob corner arrays, and optionally bypass costly shadow/rim masks while keeping drawn outlines, animation and colors.
- Skip distant actors and particles conservatively in zoomed views, honor the particle budget for flight goo and chug effects, and preserve impact rings when ordinary cosmetic effects fill that budget.
- Gameplay, Performance, Audio and Save Data share a single settings screen in both orientations. Every row is a large button with explanatory text. Reset and erase actions require confirmation; custom levels are retained.

## Tests and limitations

Eleven production-source JS regression suites passed in the development JavaScript runtime, including the new phone tests: fourteen cadence combinations, six scaled orientation cases, sixteen settings layouts, automatic tier hysteresis, persisted-setting callbacks, and effect-budget feedback. The existing orientation test checked 448 touch controls. Physics and rubble rendering comparisons also passed.

The previous all-platform revision passed its reported GitHub Actions workflows (host tests, DS, 3DS, Dreamcast, ROMs). The latest head adds a separate real Chromium smoke gate for trusted touch settings, low-end scale and a single slingshot launch; its result must be checked in CI. No physical low-end phone FPS, battery life or thermal benchmark has been conducted. For manual device QA, select Auto, then Low End, play a dense demolition level, rotate, and compare both measured render FPS and simulation SPS with the on-screen monitor.

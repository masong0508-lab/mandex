# Mandex: rubble contact lifetime and portrait walking

This continuation fixes collisions from already shattered debris and makes
walking easier to see on a portrait screen. It follows the interrupted-turn,
render-culling and DS boot work documented in
[the earlier pass](RENDER_TURN_SMOKE_2026-11-21.md).

## A broken fragment finishes one contact, then stops

Debris cleanup runs after the physics substeps. A contact can therefore mark
a fragment dead while its cell still occupies the chunk array and contact
hash. The native solver already ignored cells that were dead at the start
of a scan and dead counterpart cells, but continued the current source scan
after an impact killed that source. The browser also admitted previously
dead cells into the hash and source scan.

Both solvers now exclude dead cells when building the hash, stop subsequent
static and body contacts as soon as the source shatters, and ignore dead
counterparts already present in a bucket. The contact that causes the break
still applies its impulse, damage, effects and position correction. Cleanup,
mass recomputation, scoring and chunk retirement keep their original timing.
Surviving cells in a mixed chunk remain active.

This prevents a shattered glass fragment from damaging a second wall,
producing repeated destruction feedback or deflecting another body before
cleanup. It also removes unnecessary hash entries and later contact work.
The change uses guards in the existing loops; it adds no fields, arrays,
heap pools or render buffers.

### Native regression

`test/chunk_contact_lifetime_test.c` includes and executes the actual private
solver. Fixtures cover pre-dead hash entries, a fatal wall contact followed
by another static surface, a fatal wall contact followed by a sleeping body,
a fatal body contact followed by an unrelated third body, and a counterpart
that dies after insertion into the hash. Cleanup checks protect one-time
retirement and scoring.

The original core at `11f46df` fails 11 assertions in each numeric mode. The
guarded solver passes both modes. A separate 240-step all-live scene compares
the complete world bytes against the original implementation; the observed
hashes match in the same host/compiler configuration:

| Numeric mode | Original trace | Updated trace |
| --- | --- | --- |
| Floating point | `4ab3157c` | `4ab3157c` |
| Fixed point | `1c0346a7` | `1c0346a7` |

These hashes are a same-environment comparison, not a portable serialization
format or a universal cross-compiler golden value.

### Browser regression

`test/browser_dead_rubble_test.js` extracts the actual production solver and
compares it with a frozen copy from the same baseline. All seven focused
regressions fail on the old implementation and pass on the new one. The
fixtures include both moving and sleeping dead cells, deaths during static
and pair scans, and a cell that becomes dead after hash insertion.

The test also executes six substeps in 600 seeded worlds containing only
surviving cells. It compares complete chunk/cell state, velocities, damage,
position correction, effects, effect order and deterministic dust against
the old solver. Every resulting state matches exactly. The test harness
records external feedback calls; the real browser gates cover the full page.

## Portrait walking follows the action

The previous real-input screenshot exposed a framing problem after landing:
walking retained the distant aiming zoom. The character was small, and
rotating into portrait could reinstate the 1.1-second aim hold before the
camera followed the walker.

Portrait walking now targets 1.8 times world scale, easing toward that scale
while preserving the character's screen anchor. The camera follows the
nearby route and uses a vertical safe band so the character's head stays
below the mission strip and the feet stay above the timer and touch-control
strip during jumps, falls and zoom transitions. Rotating into a walking
scene focuses the character immediately.

Aiming retains the sling/target overview. An active drag locks the zoom so
the pointer's screen-to-world mapping stays fixed until release. Ordinary
landscape framing and the wide finale's district logic retain their existing
behavior. The implementation reuses the current camera state.

`test/browser_walk_camera_test.js` executes the production camera function
for 6,850 samples across four portrait heights and both world widths. It
checks world bounds, coordinate round trips, jump/fall headroom, zoom easing,
captured drag scale and complete camera-state parity with the old landscape
implementation. The previously captured chapter-two layout gains 2.52 times
the character scale (1.8 instead of approximately 0.714 world scale).

The real Chromium input test additionally captures the portrait walking
view and asserts its readable scale and HUD clearance. The separate Canvas
culling test compares full RGBA frames with culling enabled and disabled,
including real physics frames and varied camera positions.

## Reproduce and review

Run the dependency-light integrated suite from the repository root:

```sh
sh test/run_tests.sh
```

Focused checks:

```sh
gcc -std=gnu99 -O2 -Wno-unused-function -Icore \
  test/chunk_contact_lifetime_test.c -lm -o /tmp/chunk-contact-float
/tmp/chunk-contact-float
gcc -std=gnu99 -O2 -Wno-unused-function -DMX_FIXED -Icore \
  test/chunk_contact_lifetime_test.c -lm -o /tmp/chunk-contact-fixed
/tmp/chunk-contact-fixed
node test/browser_dead_rubble_test.js
node test/browser_walk_camera_test.js
```

The native executable also accepts `MX_CONTACT_LIVE_TRACE=1` to run only
the all-live trace when comparing a baseline checkout with the updated core.

Real browser checks require Playwright and Chromium:

```sh
python3 test/browser_canvas_cull_test.py --out-dir /tmp/mandex-canvas
python3 test/browser_input_smoke.py --out-dir /tmp/mandex-input
```

Pass `--chromium /path/to/chromium` for an installed browser. The
`browser-creator-gates.yml` workflow installs Chromium and retains images
and JSON reports. Both Node regressions and the native float/fixed fixture
are also integrated into the existing host and feature gates.

## What this establishes

The regressions establish contact lifetime correctness, preservation of
nonfatal simulation behavior in the tested fixtures, and bounded camera
framing. The changes do not establish a hardware frame rate. Console builds
and the existing DS boot/title liveness gate remain necessary release checks.
Actual DS release-ROM gameplay input automation remains follow-up work: its
emulator key mapping and semantic gameplay checkpoints need a validated run
before becoming a mandatory gate. Physical-console FPS, sound synchronization
and gameplay still require the existing on-device verification procedure.

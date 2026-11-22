# Mandex: interrupted turns, destruction rendering and DS boot verification

This continuation improves character motion and rendering within the existing
art, framebuffer and physics budgets. The tests distinguish host correctness
and isolated draw work from performance on a physical console.

## Walking reversals stay continuous

The 12-tick side-to-rear-to-side animation previously restarted whenever input
changed direction. A second reversal during the first turn therefore mirrored
an unrelated profile immediately, producing a visible snap.

Both `core/mx.c` and `web/index.html` now reverse the remaining progress with
`12 - remaining` when a turn is active, then apply the existing one-tick
advance. A fresh turn still starts with 12 ticks. Gameplay facing changes on
the input tick, so movement and punches retain their immediate response.
Neutral input finishes the current turn. There are no new animation fields,
pose textures, sprite passes, save fields or physics changes.

`test/walk_turn_interrupt_test.c` exercises actual `mx_update` and reconstructs
the yaw consumed by the three native frontends. It covers both starting
facings, grounded and airborne starting states, every interruption point,
neutral release, repeated reversals with one through seven frames between
changes, and punch direction. Each numeric mode checks 3,624 simulation
samples. The original implementation failed 1,788 continuity assertions.

`test/browser_turn_pose_test.js` executes the actual browser `walkStep`,
including its input decoder and punch timing, for 3,672 samples. It checks
that each visible angle advances by no more than one normal animation step.
The old browser implementation reproduces the same failure.

Visual review of the real Chromium input screenshots also exposed a portrait
HUD overlap: the mission strip covered part of the walking instructions, and
the top bar overpainted the walking timer. Portrait now groups the timer and
instruction line immediately above the existing touch buttons. Landscape
positions stay the same. The objective regression executes the actual HUD
draw function and checks timer, hint and button separation at four portrait
heights, including the captured 540×1169 layout.

## Destruction culling continues during screen shake

The browser multiplied shake by 0.9 each update, while both wallpaper and
debris culling required `shake === 0`. After an impact, shake could remain
nonzero long after becoming invisible, keeping the full rendering path active.

The maximum translation used by the drawing code is
`abs(shake * shakeScale()) / 2` world pixels on each axis. The renderer now
adds that displacement to its existing conservative bounds. Wallpaper keeps
each original run's starting column and stripe phase. Debris retains the
two-voxel margin plus its existing rotated-sprite padding. Only offscreen
Canvas work is skipped; debris still simulates, collides and expires normally.

The production-loop transform test compares visible sprite identities,
positions and paint order with a complete reference draw. It tests landscape,
portrait, tall portrait, three camera positions/zooms, the three shake options,
five amplitudes (including nearly decayed shake and an oversized edge stress
case), and nine shake directions: 1,215 combinations. Additional rotated
fixtures straddle each viewport edge and its corners.

In the seeded crowded scene, the reference submits 1,472 live debris sprites.
The optimized loop submits 247 without shake and 250 with amplitude 16. This
is approximately 83% fewer debris `drawImage` calls in that fixture, not a
whole-game frame-rate measurement. Normal full-world camera mode retains the
complete drawing path.

The separate Chromium gate compares the complete production `draw()` canvas
against a reference with wallpaper/debris culling disabled. It snapshots
stateful cloth and toast drivers before each render so the comparison is
between the same frame. It requires identical RGBA bytes, including camera
edges, room details and real collapse frames. Its live-input companion uses
the page's mouse and keyboard handlers for launching, walking, reversing,
jumping and changing orientation. Both retain screenshots and JSON reports
as CI artifacts.

The [first real Chromium run](https://github.com/masong0508-lab/mandex/actions/runs/44967331543)
passed all 177 full-frame comparisons: 385,171,200 RGBA bytes compared, zero
different pixels, and no page errors. Across the test corpus it skipped
88,904 image submissions and 109,970 rectangle fills. Its companion input
smoke passed 914 fixed simulation steps, observed 95 trusted keydowns and
matching releases, and verified launch, collapse, walking, jumping and both
orientation transitions. Those screenshots prompted the portrait HUD
spacing correction described above.

## 3DS voxel raster work

Standing and flying tiles use 3- or 4-pixel footprints. The new raster path
clips that footprint once, then walks contiguous bytes in the column-major
BGR framebuffer. The former inner loop repeated coordinate checks and
framebuffer-address multiplication for every pixel in both stereo views.
Alpha blending, damage color, brightness and tile submission order are
unchanged. The reference path remains available through
`-DMX_N3DS_TILE_CLIP_RANGES=0`.

The independent reference rasterizer in `test/n3ds_render.c` compares every
byte, including guards around the framebuffer, in 194,688 cases covering:

- Eight materials and four texture variants.
- Three damage stages and six brightness values.
- Both tile sizes, fully visible tiles, fully hidden tiles and all edges.

Actual aim/rubble/stereo render probes also require identical framebuffer
hashes. The existing frontend gameplay harness reaches a level-one victory,
checks stars in both eyes and the saved best score, and returns to the title.

Six alternating host trials, each drawing 500,000 tiles, measured 16.968–17.958
ms for the reference and 15.294–15.908 ms for the clipped path, about 10.3%
lower median isolated raster cost in that run. The two host binaries had
identical `.data` and `.bss`; executable text increased by 64 bytes. These are
host measurements. They establish neither console FPS nor whole-game speed.

## DS smoke checks can fail on a bad boot

The old workflow tolerated emulator exits and screenshot failures, allowed
the smoke job to fail, and packaged ROMs independently of its result. The
updated workflow runs console builds on relevant pull requests as well as
main and requires successful DS smoke before packaging.

During the bounded 90-second emulator phase, the smoke step checks process
liveness before and after each capture. It captures the 256×384 emulator
client, excluding the desktop and changing FPS window title. The validator
requires both screens to contain at least 16 colors and 5% lit pixels, and
at least 64 client pixels to change between adjacent captures. Only one
screen needs to animate; the level picker can legitimately remain still.
Screenshots, window diagnostics and emulator logs are uploaded on failure.

The validator uses only Python's standard library. Seven regression tests
cover animated captures, frozen content, blank/diagnostic screens, one black
screen, desktop-sized or corrupt captures, binary pixel parsing, and missing
or single-frame inputs. Shell integration checks also reject a dead process,
missing window or failed screenshot command. Actual baseline title captures
pass with 101 colors on the top screen and 79 on the lower screen, with more
than 22,000 changed client pixels between consecutive samples.

This is a boot/title liveness gate. It does not establish that a release ROM
can complete gameplay, that sound is synchronized on hardware, or that a
physical DS meets its frame-rate target.

## Reproduce

Run the integrated suite from the repository root:

```sh
sh test/run_tests.sh
```

Focused browser and boot checks:

```sh
node test/browser_turn_pose_test.js
node test/browser_rubble_draw_test.js
python3 test/ds_smoke_test.py
```

The real browser gates additionally require Playwright and Chromium:

```sh
python3 -m pip install playwright
python3 -m playwright install chromium
python3 test/browser_canvas_cull_test.py --out-dir /tmp/mandex-canvas
python3 test/browser_input_smoke.py --out-dir /tmp/mandex-input
```

Pass `--chromium /path/to/chromium` to use an installed browser. These checks
run separately from the dependency-light host suite in
`browser-creator-gates.yml`.

The 3DS host harness accepts `N3DS_TILE_CLIP_PROBE=1` for exact pixel checks
and `N3DS_TILE_CLIP_BENCH=1` additionally for paired host timings. The
`render-fastpaths.yml` reference/optimized compile now toggles the tile path
alongside its existing render comparisons. `feature-smoke.yml` and the full
host runner both include the new tile and interrupted-turn probes.

No hardware FPS figures are inferred from these tests. Performance work
should continue with the existing on-device FPS, simulation-step and draw-ms
diagnostics using the same level, camera, outfit and collapse sequence.

# MANDEX

An Angry Birds-style game: a sticky dad-bod man in red underwear and a pink towel-cape gets flung into voxel buildings and
wrecks them with sandbox-style destruction physics, to evict the landlords inside.  Tasteful, silly, and a tad explicit.

![poses](docs/man_sprites.png)

One game, four builds: a browser version with its own JS physics, and three consoles sharing one C core:

| Port | Where | State |
| --- | --- | --- |
| Browser | `web/index.html` (single file, open it in any browser) | playable, has its own JS physics |
| Nintendo DS | `ds/` (devkitPro / libnds, 20.12 fixed point) | written and tested on the host with stub libnds, **never built or run on a real DS or emulator** |
| Nintendo 3DS | `3ds/` (devkitARM + libctru, float, software-drawn) | furthest behind: circles-and-rectangles man, no sound; **never built or run on a 3DS or emulator** |
| Sega Dreamcast | `dreamcast/` (KallistiOS, float) | same: host-tested with a software rasteriser, **never built for real** |

`core/mx.c` is the shared C game core for the three consoles (a 120x60 voxel grid, rigid chunks, a wobble lattice with
8 block kinds and 60 levels (20 story levels, 20 chapter 2 levels, then 20 chapter 3 levels; same layouts and mechanics as the web game); `core/mx_meta.c` holds the stars, achievements and outfits rules), `art/` holds the generated pixel art and the idle-gag player, `tools/` the sprite generator,
`test/` the host tests.  See [PORTING.md](PORTING.md) for the full status, design notes and what is not verified.

## What is in it

- **Destruction physics**: voxel buildings sit on springs (they sway, crack, snap and topple); wood, brick, stone, glass,
  TNT, steel, rubber and sand all behave differently.  Rubber boings the man back.
- **The man**: 54 pixel-art poses.  Idle gags (belly slap, butt wiggle, belly scratch, burp, fart, nose pick, yawn, and
  a cape taunt where he steps round on his feet, whips off his cape and flags a real cloth towel while he wags his butt), gross-out effects (stink clouds, flies,
  boogers, sweat), a sleep with snot bubble after 20 s, a victory butt-wiggle dance and a sad slump on defeat.
- **The towel** is a small cloth simulation (`core/cloth.c`, recipe in `docs/CLOTH.md`): it flutters, folds and turns edge-on
  with his body, the same in the browser, on the DS and on the Dreamcast.
- **Progress** (all consoles, this session only until save data lands): 1-3 stars per level, 25 achievements, outfits.  The DS has an
  achievements list with progress bars and an outfits screen (swatches, padlocks, how to unlock).  Level locks (first 3 open, then in
  order) are built but switched off (`-DMX_LEVEL_LOCKS`) until saving exists; on the DS title L+R+SELECT is the unlock-all cheat.
- **Options** (browser): a simple OPTIONS screen from the main menu.  *Collapsible buildings* keeps fallen rubble physical (it keeps slumping and
  toppling) until the level ends; off, rubble settles and locks solid.  Either way, damaged walls crumble after each run.
- **Body jiggle**: his belly and rear are damped springs, kicked while he flies, hits something or drops, plus an
  impact squash.  It is cosmetic and never touches the physics.
- **Eviction combos**: chain landlord defeats within 2.5 real-time seconds for a capped +1,000/+2,000/+3,000/+4,000 score bonus and increasingly outrageous adult-comedy stingers. The browser and three consoles share the same score rules; it never affects saved-file structure or adds an allocation.
- **Comparable runtime diagnostic overlays across all four ports**: DS **L**: ARM7 VBlank-observed render FPS, simulation steps per second and top/bottom/DMA timing; 3DS **L+R**: observed FPS/SPS from libctru time; Dreamcast **both analog triggers**: observed FPS/SPS from KOS time; browser **F3**: requestAnimationFrame FPS, actual gameplay update calls/s, and percentage of real time advanced by the clamped game timestep. The overlays operate only while enabled/held and do not claim that host microbenchmarks equal hardware FPS.
- **DS real-time simulation recovery**: on the bare-metal ROM, the ARM7 VBlank counter reveals when drawing consumed two or three video periods. Rather than silently slowing the game to 30 simulation steps/s, the ARM9 can now execute 2-3 **actual 1/60-second physics steps** in a frame if its measured previous physics cost is low enough. Each step's collision/explosion/meta events are processed immediately (never overwritten by a later step), and stalls of more than four VBlanks or expensive physics updates do not trigger a catch-up storm. The L debug meter reports observed simulation SPS and rendered FPS independently. Host regression validates the bounded decision policy.
- **Tiny opaque tile performance path**: frequent 2×2 and 4×4 pixel tiles on both DS screens now bypass the generic inner loop. A separate audit compares 51,200 random damage/wall draw calls between old and new builds and checks pixel hashes and buffer guards; `-DMX_DS_FAST_TILES=0` disables it for A/B measurement. The appearance and memory budget are unchanged.
- **Emulated DS FPS captures**: [DS emulated FPS capture](.github/workflows/ds-emulator-fps.yml) makes test-only auto-start ROMs for levels 1, 40, and 60 with the real ARM7 VBlank FPS/SPS overlay enabled and captures DeSmuME screenshots after several seconds. These numbers describe the emulator, not confirmed DS hardware speed. No test flag is enabled in release ROMs.
- **Adaptive extra bottom-screen relief under overload**: when measured combined CPU cost indicates an over-budget frame and nobody is aiming or touching the lower screen, redraw the secondary scene at 15 Hz; preserve up to 60 Hz for active controls and the upper display. The [emulated FPS workflow](.github/workflows/ds-emulator-fps.yml) now compares baseline (`MX_DS_OVERLOAD_BOTTOM15=0`) versus adaptive (`=1`) ROMs for three levels, saving on-screen FPS/SIM screenshots for both. This is a reversible compromise between secondary animation rate and gameplay responsiveness.
- **DS second-screen rate reduction with A/B timing**: both DS software-rendered screens previously refreshed every game frame. The top stays full-rate, but the lower overview/aim display now refreshes at 30 Hz when untouched; stylus dragging and D-pad aiming immediately restore full-rate bottom drawing. Skipped lower-screen frames also skip their VRAM DMA transfer. CI compiles the old 60 Hz bottom path and the new 30 Hz path in the same run, publishing paired *host CPU cost* measurements by level. This does not promise 60 rendered DS FPS on physical hardware.
- **Measure real DS FPS rather than assume 60**: on a bare-metal ROM, press **L** to see actual rendered FPS and simulation steps/s (ARM7 VBlank clock), missed VBlanks and phase timing (ARM9 timer). The new [DS bottleneck audit](.github/workflows/ds-fps-audit.yml) profiles real game-loop CPU cost in eight representative levels split by input, physics, event processing, top draw, bottom draw and VBlank/DMA; host microseconds are **not** hardware FPS. Adaptive quality no longer copies the bottom bitmap into VRAM when its redraw is skipped.
- **3DS gameplay night/parity + deterministic level profiling**: Chapter 3 now uses a proper moonlit blue-purple sky, nighttime clouds, city lights, and darker layered hills/trees instead of reusing the daytime palette. Forty-one snowy ridgeline samples are computed just once for both stereo eyes instead of recalculating trigonometry every frame. A dedicated [gameplay profiling workflow](.github/workflows/gameplay-perf.yml) measures every level for 120 deterministic physics frames in fixed and float modes; these are host measurements, not console FPS.
- **Measured cutscene performance**: the shared 8-bit renderer fast-fills full-width bands and entire frames as single memory operations, with 480 reference-rectangle checks added to the pixel-equivalence suite (1,920 shapes total). Every fast cutscene CI run reports measured host-only mean/p50/p95/max rendering times by story mode plus an artwork checksum in an attached text artifact. These results are not hardware FPS measurements.
- **Cinematic acting and sound parity**: DS/3DS/Dreamcast now share rooted gestures, one-eyed winks, recoil and nod animation, lightweight parallax city lights, detailed indoor garage artwork and corrected first-frame sound cues, with an audio regression assertion. The web build gets equivalent lamplit skyline art and single-eye winks. Browser romantic lighting is bounded to its actual glow area.
- **Visual cutscene contact sheets**: the [Cutscene gates](.github/workflows/cutscene-gates.yml) Actions workflow runs fast browser and console checks, renders a representative three-frame storyboard from six major story beats, converts the sheets to PNG with only Python's standard library, and uploads the PNGs as a `mandex-cutscene-storyboards` artifact for visual review. These previews are not new in-game textures and consume no runtime memory.
- **Cartoon rear-view slapstick**: Enlarged rear-view comedy panels, elastic fabric/fur wobble, tail and costume acting, harmless rubber suction-cup misfires, laugh captions, and timed boing cues in origin, wolfman, and chapter-complete cutscenes across the browser and DS/3DS/Dreamcast. No sexual penetration, additional console assets, framebuffers, or heap allocations.
- **Adult-comedy romance cutscenes**: Gerald and Brenda share exaggerated winks, glamorous fully clothed designs, over-the-top flirtation, a briefly silhouetted kiss, and a moonlit fade-to-black. Browser effects are rendered directly on the existing canvas; console cutscenes use the shared low-memory 8-bit renderer. The content is suggestive, not graphic sexual depiction. The intended adult tone is not an official age rating.
- **Illustrated cutscene artwork pass**: 15 compact hand-designed pixel-art emblems, shingled homes and detailed props, layered starry skylines and moon craters, story-specific rain, paper scraps, sparks, confetti and slow atmospheric motion. The DS/3DS/Dreamcast share an 8-bit storyboard renderer; the browser displays matching emblems, sliding illustrated scene cards, beat-specific ink swishes and impact captions. No extra handheld framebuffers, atlases or heap allocations.
- **Cinematic cutscene pass**: eased camera pushes, comic impact accents and dialogue progress on the browser; lightweight atmosphere, shot-decay camera shake, and narration progress cues in the DS/3DS shared intro.
- **Handheld visual-parity pass**: DS and 3DS dress destructible room backdrops with multi-pane windows, curtains, furniture, surviving-roof highlights and street signage. The 3DS gains a title poster and demolition meter. Its 3px and 4px tile atlases stay cached separately, so changing screens no longer rebuilds materials each frame (about 35 KB extra static RAM, no heap).
- **Handheld performance pass**: bare-metal DS uses DMA3 instead of thousands of CPU writes when copying both 8-bit framebuffers to VRAM. The audio sequencer catches up to ARM7's independent VBlank count if the ARM9 game frame is delayed, keeping tempo close to real time. Under rendering pressure the DS reduces decorative bottom-screen updates while preserving full-rate stylus dragging. 3DS culls offscreen world voxels/wallpaper and uses a one-time 6 KB hill-height lookup instead of per-frame trigonometry; stereo camera smoothing updates just once per frame.

## Build and test

```sh
sh test/run_tests.sh            # host tests: needs only gcc (a few minutes: includes a 560-run robustness sweep)

cd ds && make                   # -> mandex.nds   (needs devkitPro: devkitARM + libnds)
cd dreamcast && make            # -> mandex.elf   (needs KallistiOS: source its environ.sh first)

pip install numpy pillow playwright && python -m playwright install chromium
python tools/gen_manspr.py      # re-render art/manspr.h from web/index.html's own drawing code
```

Controls: DS stylus drag-and-release to fling, tap in flight to belly-flop; 3DS the same on the bottom screen; Dreamcast analog stick to pull, A to launch
or flop.  Chapter 2 walk levels (Walk-Up Wreckage, Collection Day, Batter Up): once he lands on his feet, DS/3DS d-pad
walks, A / up jumps, B / Y punches, X / down belly-slams, L gives up (3DS also has touch buttons); Dreamcast d-pad or stick walks, A jumps, B punches, X slams,
L trigger gives up.  Landlords fighting back can be switched off: DS options screen, 3DS Y on the title, Dreamcast Y on the title (X on the pause screen).
Full list in [PORTING.md](PORTING.md).

## Workflows (`.github/workflows`)

| Workflow | When | What |
| --- | --- | --- |
| `tests.yml` | push, PR | runs `test/run_tests.sh` |
| `build-ds.yml` | push / PR touching `ds`, `core`, `art` | builds `mandex.nds` in the `devkitpro/devkitarm` image, uploads it |
| `build-dreamcast.yml` | push touching `dreamcast`, `core`, `art`, or manual | **experimental**: builds `mandex.elf` in a community KallistiOS image, allowed to fail |
| `sprites.yml` | manual | re-renders the sprites from the browser version and uploads `art/manspr.h` |
| `pages.yml` | manual | publishes `web/` to GitHub Pages |
| `release.yml` | tag `v*` | tests, builds the DS ROM, attaches the ROM and `mandex.html` to a release |

The DS and Dreamcast build workflows are the first real test of those Makefiles: expect to fix a few compile or link
problems on the first runs.

## Honest status

Everything compiles and passes on a normal PC compiler, but the console builds were written without a toolchain, no
emulator or hardware has run them, DS frame time is unknown (the in-game profiler is on `L`), and none of the sounds
have been heard.  The Dreamcast port has no sound yet.

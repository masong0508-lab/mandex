# MANDEX console ports - status

One shared C core, three console front-ends (DS, 3DS, Dreamcast) plus the browser build, which has its own JS physics and is the reference.

```
core/mx.h, mx.c            portable game + physics (no platform calls, no malloc)
core/cloth.h, cloth.c      the towel: small cloth simulation shared by both consoles (docs/CLOTH.md)
ds/source/main.c           DS game loop: dual-screen cameras, input, particles, HUD, title/pause/win/lose
ds/source/gfx.c, gfx.h     DS software renderer toolkit: palette, material tiles, parallax sky, font, sprites
ds/source/sfx.c, sfx.h     DS sound: glide/arpeggio effects on PSG + noise channels, small chiptune loop
dreamcast/main.c           Dreamcast front-end (KallistiOS PVR) - still the first-cut one
art/manspr.h               GENERATED man pixel art shared by both consoles (tools/gen_manspr.py)
tools/gen_manspr.py        builds art/manspr.h from the HTML prototype (needs Playwright, only for this step)
art/title.h                GENERATED title picture for the DS (run-length coded, 256x192) and the Dreamcast (320x240 as flat quads) (tools/gen_title.py)
tools/gen_title.py         builds art/title.h + docs/title_art.png from the HTML title (drawTitlePicture in web/index.html)
test/                      host regression tests, robustness sweep, DS/DC host harnesses
docs/                      screenshots rendered by the host harness (real DS drawing code)
```

## ROADMAP PROGRESS (Nov 2026): DS first
Work follows the "Mandex Porting Roadmap" (16 ports) and its status update.  Done so far, with the patch that did it (apply in this order):

| Port | State | Patch / where |
| --- | --- | --- |
| 1 Stars, best scores, build plumbing | done on the host, never run on a 3DS or emulator.  `mx_stars_for()` in core (DS end panel and 3DS use it).  3DS: three sparkle stars on the win panel (lit gold, unlit grey), blinking NEW BEST! (old best read before `mx_meta_win`), `BEST 000000` / `NOT PLAYED YET` under the level name on the title; `3ds/Makefile` now builds `cloth.c` and has `../art` on the include path (ports 8 and 9 need both).  Host harness `test/n3ds_render.c` + `test/stubs/3ds.h` run the real `3ds/main.c` loop with scripted pad input and check the star count (3, 2, 1 men left), NEW BEST! and the title line, with the 3D slider at 0 and at 1 (both eyes); in `run_tests.sh`.  **Colour bug found while doing it:** `RGB()` put red in the low byte, but the 3DS BGR8 framebuffer wants blue in byte 0, so every colour on the 3DS build had red and blue swapped; the macro now puts blue in the low byte (the harness screenshots showed it).  Not verified: libctru, the Makefile build (CI `n3ds` job is the first test), anything on real hardware. |
| 2 Level unlock order | DS done: `mx_level_unlocked()` + `unlock_all` in core, the DS picker shows LOCKED and refuses to start; all behind `-DMX_LEVEL_LOCKS` (off by default, turn it on when port 5 saves).  DS cheat: **L+R+SELECT on the title** toggles `stats.unlock_all` for the session (never saved).  3DS and Dreamcast pickers still to do | `mandex-ds-unlock`, `mandex-ds-cheat` |
| 5 Save data | core part done (steps 1 to 5): `best[]` moved into `MxStats` (set by `mx_meta_win`, so no front-end keeps its own copy; the DS and 3DS arrays are gone, the Dreamcast never had one), `mx_meta_pack` / `mx_meta_unpack` / `mx_crc32` in `core/mx_meta.c`, layout documented in `core/mx_meta.h` (175 bytes for 20 levels, little-endian, `MXS1` + version 1 + level count + CRC32).  `test/save_test.c` (float and fixed).  **Nothing writes or reads a save yet**: DS card chip (port 11), 3DS SD card and Dreamcast VMU (steps 6 to 9) still to do.  Call `mx_meta_unpack` before the first `load_level()` at boot; for the 3DS NEW BEST! line read the old `stats.best[level]` before `mx_meta_win` | `mandex-save-core` |
| 4 Achievements and outfits screens | DS done.  Core: `mx_ach_progress()`, `mx_ach_unlocked()`.  Achievements: a paged list with progress bars (tap the ACH counter on the title).  Outfits: a 4x3 swatch grid with padlocks and the unlock hint of the highlighted outfit, worn one marked WORN (tap the OUTFITS counter on the title; tap or A wears, B or BACK returns).  3DS and Dreamcast screens still to do | `mandex-ds-ach`, `mandex-ds-outfits` |
| 10 Outfit parity (Working Man) | core + DS done (host only): `MX_NUM_OUTFITS` is 12, Working Man is appended as index 11 (always owned, the browser's blue `#3f5f8f` boxers and `#c9ced6` towel; `MxOutfit.work` flag; saves store the outfit as an index so old saves still load).  The DS outfits screen shows a small WORK marker on it.  `test/outfit_test.c` (in `run_tests.sh`, float and fixed): every unlock rule, Working Man, a save round trip with outfit 11, and no Walk-in Closet / Birthday Suit deadlock.  **Not done:** the gold briefs logo and the rear fly patch stay on the sprites: the logo shares sprite palette entry 18 (`MS_PAL[17]`, 226,181,60) with other gold pixels, so it cannot be recoloured separately without regenerating `art/manspr.h` / `manidle.h` via `tools/gen_manspr.py` (needs numpy, Pillow, Playwright and a `WORK` render with a 40th colour; not possible here).  Mint Condition, Birthday Suit and the missing Cheeky Thong keep their console rules (see the roadmap).  Dreamcast (`recolor_man` tints from `mx_outfit_shades`) and 3DS pick the new outfit up through `MX_NUM_OUTFITS` but were not looked at. |
| 11 DS save data | written, never run on a DS or emulator: `card_save_read` / `card_save_write` in `ds/bare/hw9.c` talk SPI to the cartridge's 8 KB EEPROM (commands 0x03 read, 0x06 write enable, 0x02 page write, 0x05 status; 32-byte pages; a missing chip, a FLASH chip or a 512-byte chip fails the write-enable check and saving silently turns off).  `ds/source/main.c`: `save_load()` before the first `load_level()`, `save_sync()` packs the stats and writes only when the bytes changed, at the end of a level (win or lose), on returning to the title and when leaving it for a level, never in play.  Host: `ds_render` with `MX_SAVE_PATH=<file>` models the chip as an 8192-byte file (in `run_tests.sh`).  **Unproven:** the AUXSPICNT / EXMEMCNT bit values were written from memory of GBATEK and libnds, and the ROM header does not declare a save type (melonDS and DeSmuME may need an 8192-byte `mandex.sav` next to the ROM; record what each does).  `-DMX_LEVEL_LOCKS` can be turned on once this is seen working. |

DS notes for the next person:
- The achievements and outfits screens are sub-screens of the title (`ach_open`, `out_open`), not new `app` states.  Both draw on the bottom screen only;
  the top screen keeps the title picture (the title swaps the top engine's palette, so a live man preview is not possible there).
- Outfit swatch colours use main-engine palette slots 224..255 (two per outfit, `C_SW0`).  The man owns 160..198 (port 10 wants 199 for the logo); the
  title only touches the sub engine.  The grid holds 12 outfits: a 13th breaks the build on purpose (`out_grid_fits` in `ds/source/main.c`), so paging has to be designed then.
- Outfit hints come from `mx_outfit(i)->hint` only.  Rainbow Rump is flagged `rainbow` in core, but the DS `apply_outfit` does not implement it: wearing it gives plain red.
- On the title, L and R act on **release** (L = profiler, R = next outfit) so the L+R+SELECT cheat can latch first.  Elsewhere L still toggles the profiler on press.
- Host checks: `test/meta_test.c`, `test/ach_test.c`, `test/stars_test.c`, and in `ds_render` the scripted `OUTFIT_SHOTS=1` and `CHEAT_TEST=1` runs (both in `run_tests.sh`);
  screenshot `docs/ds_outfits.png`.  Nothing has been run on a DS, an emulator, or looked at on a real screen.
- Still stale below: the older passes talk about 7 levels and "both consoles"; the game now has 20 levels, 25 achievements, 11 outfits, the air valve and three power-ups (beans, towel, splat).

## IDLE / GROSS-OUT / JIGGLE pass (DS + Dreamcast + HTML)
New man frames (27 in all, `art/manspr.h`, made by `tools/gen_manspr.py` from the HTML art): belly bounce A/B, belly
scratch A/B, burp A/B, nose pick A/B, yawn, sleep A/B (snot bubble), sad, back view centre/left/right (butt wiggle),
fart A/B.  Each frame also carries belly and rear "blob" centres and radii for the jiggle.
- `art/manidle.h` (header only, shared by DS and DC): 7 gags (belly slap, butt wiggle, belly scratch + sniff, burp,
  fart, nose pick + flick, yawn) are tables of `{frame, ticks, fx}`.  A random gag starts after 3-5 s of no input
  (never the same one twice), he falls asleep after 20 s, any input wakes him.  The front-end only has to call
  `mi_update()` each tick, draw `mi_frame()` and react to `mi.fx` (sound / particles).  `mi_dance_frame()` is the
  victory butt-wiggle loop.  `test/idle_test.c` checks the tables.
- Gross-out effects: green stink clouds (burp, fart, a thin trail while flying), flies after a fart, a flicked
  booger, sweat when the sling is pulled hard, Zzz while asleep.  DS also has sounds (slap, boing, grunt, burp,
  fart, flick, yawn, snore: all PSG/noise note lists in `ds/source/sfx.c`) and "BUUURP!" / "PFFFFT!" popups.
  Dreamcast has no sound yet, so its gags are visual only, and no text popups.
- Win screen: two men doing the butt-wiggle dance.  Lose screen: the sad pose.  Title (HTML, DS, Dreamcast): the man dropping butt-first onto a very surprised landlord, no tagline.  The DS title is the picture on the top screen in its own sub-engine palette slots 64.. (swapped in only while the title is up) with the logo and the control hint drawn on top; the Dreamcast now has a title screen too (logo baked into the picture, blinking PRESS START, START or A begins).
- Jiggle physics (cosmetic, in `core/mx.c: man_jiggle`, so all ports agree): the man's belly and rear are two damped
  springs (belly K400/D7, rear K250/D6, clamp 1.5 cells) kicked by his change of velocity while flying, when he hits
  something and when he drops; hits also squash him about 12% along the impact direction.  It never touches the
  rigid-body or lattice simulation.  DS draws it by shifting pixels near the belly / rear blob (`blit_frame` jiggle
  path in `gfx.c`); Dreamcast shifts the quads of each pixel run.
- Host checks: `test/wobble_test.c` [7] (spring peaks, calm after the hit), `test/idle_test.c`, DS idle screenshots
  (`IDLE_SHOTS=1 ./ds_render ...`), Dreamcast host render (`dc_render`: idle sheet, jiggle shots, win, lose).
- Faces / back view (latest pass): the generated heads were only ~12 px wide and looked muddy, so `tools/manhead.py`
  holds hand-pixelled heads that replace them: a 19x19 front head (12 expressions: cool shades, scream, happy, bliss,
  burp, tight-shut, cross-eyed, yawn, sleep, sad, dizzy ...), a side head for flying / diving, a small head for the
  stuck pose, and a back-of-head (hair whorl, ears, neck) for the butt-wiggle and fart poses.  `gen_manspr.py` pads
  the sprites, wipes the old head, paints these, then trims.  5 extra palette colours are used only by these faces
  (hair shine, blush, iris, lip, tooth): the palette is now 27 entries (DS loads them at `C_MAN0`=160..186).
  The butt is re-shaded in the generator (`shade_butt`): two round cheeks, dark cleft, highlights.  Love handles
  and the new face / butt look in the HTML come from `drawManFrame` itself (backup `mandex_pre_look.html`).
- NOT verified: sounds were never heard; no real DS / Dreamcast run, so speed and the look on real screens are unknown.
  The jiggle blit on DS costs extra pixels only while he is actually moving.

## NEW MAN SPRITES pass (DS + Dreamcast)
- The man now matches the HTML prototype: curvy dad bod, belly roll, chest hair, tube socks, brown shoes, briefs with the gold
  logo, towel cape, gold shades + handlebar mustache. `docs/man_sprites.png` shows every frame (zoomed 4x).
- **10 frames**: aim idle (2, belly sways), drag/yelling, flex (2), flying (2, legs + cape), belly-flop ball,
  stuck/dizzy star (2). DS: dragging the man in the sling switches to the yelling frame; the HUD shows head-and-shoulders busts.
- They are **generated, not hand-drawn**: `python3 tools/gen_manspr.py` runs the HTML's own drawing code in headless Chromium,
  shrinks it to chunky pixel art (outlined, fixed 22-colour palette) and writes `art/manspr.h`, which both consoles include.
  The faces are then pixelled by hand in that script (the generated heads were too small to read). Edit the script, rerun, done.
- DS: one palette block (`C_MAN0`), nearest-neighbour blit at any zoom (art is authored at 4 px/cell). Dreamcast: one quad per
  horizontal run of the same colour (~300 quads per man).
- Not done / caveats: frames are not rotated in flight (the HTML rotates the man; the consoles show him level); no walk/win/lose
  animations beyond the frames above; looked at in host screenshots only, not on a real DS screen or on a Dreamcast.

## TOWEL CLOTH + STEP-TURN + SMALLER HEADS pass (HTML + DS + Dreamcast)
- **The towel is a real cloth now** (it was a drawing). A 7x3 grid of particles with Verlet integration, sqrt-free distance
  constraints, a wind gust that runs late along the cloth, and a normal-drag term so it flutters like cloth, not like a rope.
  The exact recipe is `docs/CLOTH.md`; the browser (`class Cloth` in `web/index.html`) and the consoles (`core/cloth.c`,
  float or 20.12 fixed, no platform calls) follow it step for step. `test/cloth_test.c` checks it stays finite, keeps its
  length (gag motion < 25% stretch), moves, and gives the same picture in float and fixed.
- It is drawn as 12 depth-sorted 3D quads (front pink, back darker pink, 3 shade steps, cross stripes, outline on the outer
  edge only), so it turns edge-on and foreshortens with the body. Spin: he flies it like a flag; back view: he waves it like a towel.
- **Consoles**: the sprites are generated WITHOUT a towel (`NOTOWEL = true` in the generator) and the HTML exports where the
  raised fist is in every frame (`TOWEL_HAND` -> `MS_HAND` in `art/manspr.h`, quarter design units, 32767 = no towel).
  `ClothActor` (core/cloth.c) follows that hand, so a frame change eases instead of kicking, and waves it a little inside the fist
  because the sprite arm is baked. DS: `gfx_cloth` (scanline triangle fill into the 8-bit framebuffer; quads behind the body are
  drawn before the sprite, the rest after). Dreamcast: `draw_cloth` (PVR strips + outline segments). Colours: `MS_CL_COL`, `MS_CL_OUT`.
- **He now turns with his body and legs** (HTML `drawSpin`, applied by `tools/patch_turn.py`): the turn is 13 frames forward
  (`SPIN_0..12`, every 15 degrees) and 11 back (`SPIN_R1..R11`), with the feet hopping round in two steps (the right foot first,
  then the left; each stays planted between hops), two-bone leg IK, hips lagging the shoulders, the head leading them, a lean
  toward the planted foot and a small crouch while a foot is in the air. The cape-taunt gag in `art/manidle.h` plays them
  (`MI_MAXSTEPS 40`), then the butt-wiggle pair, then back. The live browser version blends the same keys (`spinLive`).
- **Glitch fixes in the spin sprites**: the generator used to find the head by searching for the topmost pixels, which locked onto
  the raised hand (a duplicated head, rectangular holes, loose top-knot specks). It now takes the head position from the model
  (`spinSway`), wipes it with an ellipse (and the top-knot), paints one fixed back head, then despeckles.
- **Smaller heads on all console sprites** (`tools/manhead.py shrink`; the browser head is unchanged on purpose).
- Other changes: `core/mx.c` has a safety net found by CI (a Level 6 chunk that never settled: it now settles after 8 s and a
  stuck chunk is dropped after 10 s); the robustness sweep is 560 sequences; the HTML no longer throws on the title screen
  (`man` is not defined before the first level) or when a title-gag dust puff meets a missing grid.
- Sprite sheet: 54 frames, 39 palette entries (`docs/man_sprites.png`). DS draws the cloth at runtime only while a frame with a hand is
  on screen (about 430 constraint solves per tick in fixed point, `divf32` each).
- NOT verified: no real DS / Dreamcast build or run (CI will be the first), DS speed with a towel on screen is unknown, the HTML look
  was judged from screenshots only, the towel physics was never compared by eye on real hardware, sounds still never heard.

## WOBBLE + BLOCK KINDS pass (all ports share this: it lives in `core/mx.c`)
**What you will see**
- Buildings now **wobble**. Every standing voxel is a mass on springs to its 4 neighbours. A hit, blast or landing
  rubble injects velocity that travels through the structure as a wave; tall things lean further (inverted-pendulum
  term), then settle. Voxels are drawn at their displaced position (DS, Dreamcast and HTML all do this).
- If a joint is stretched past its break strain it **snaps** (dark crack line on the voxel, dust + creak). Snapped joints
  count as gaps in the support check, so a building can shear along the crack and the top half **topples**,
  leaving with the speed and spin it had while swaying.
- Landlords ride the wobbling floor under them (`mx_target_offset`).
- **8 block kinds**, each with its own density, health, stiffness, damping, break strain, bounce, friction, brittleness:

| kind | feels like |
|---|---|
| wood | light, bendy, creaks, sways a lot |
| brick | medium; stiff, snaps when sheared hard |
| stone | heavy, barely moves, hard to break |
| glass | light, tiny break strain: windows pop first, shards bounce |
| TNT | as before (chain blasts); wobbles like light wood |
| steel | very heavy and strong, rings the longest |
| **rubber** (new) | pink; the man **bounces** off it (e=0.92) and it jiggles for seconds |
| **sand** (new) | no cohesion: falls, piles at 45 degrees, slumps when dug out, never becomes a chunk |

- Two new levels: **6 Bouncy Castle** (rubber walls) and **7 Sandcastle Shakedown** (sand mounds + houses).

**Core changes**: `MatProp` table, lattice (`wob_*`, `wobble_step`, `wobble_check`, active-cell list), `sand_step`, `bounce_rubber`,
joint-aware `support_pass`, chunks inherit displaced position / velocity / spin, per-material restitution + friction,
`EV_CRACK` / `EV_BOUNCE` events, `mx_material_hp()`, `mx_cell_cracked()`, `mx_target_offset()`, `mx_set_wobble()`.

**Front-ends**: DS draws offsets + crack states + rubber/sand tiles + new particles/sounds (CRACK, BOUNCE);
Dreamcast draws offsets/cracks/new particles; HTML prototype has its own JS port of the same lattice.

**Verified on the host machine** (`sh test/run_tests.sh`)
- All 7 levels stand untouched for 10 s: no chunks, no cracks, lattice asleep.
- Per-kind sway test (6x30 tower, one shove): wood 0.52 cells peak, settles ~3.6 s; brick 0.25 / 1.9 s; stone 0.15 / <1.7 s;
  steel 0.14 / 4.4 s; rubber 0.91 / 5.5 s; glass pops windows first.
- Forced shear on a 40-tall tower: the upper 155 cells leave moving sideways with spin and fall as one piece.
- Sand mound slumps into a dug hole and stands untouched; man thrown at a wall: brick stops him (wall takes damage),
  rubber sends him back at ~41 cells/s with no wall damage.
- **560 random-shot sequences (280 per math mode) never hang**. Two real hangs were found and fixed on the way: a
  sheared-off slab that stayed skewed forever (fixed by a weak pull-home spring) and the lattice never going to sleep.
- With the lattice switched off the sweep also never hangs; libnds-math path is still bit-identical to software fixed point.
- Real DS main loop on the stub libnds on levels 1, 4, 6, 7; screenshots `docs/ds_7..9`.

**NOT verified / honest caveats**
- **DS speed is the big unknown.** Estimate only: the lattice is awake ~79% of play frames, ~340 standing voxels, 3 sub-steps
  per frame, roughly 300k ARM9 cycles (~4-5 ms) per frame on top of the old solver. Never measured on hardware or an emulator.
  The DS code already drops substeps first and then **switches the lattice off** when the solver exceeds ~9 ms (and brings it back
  after 2 s of headroom). Check the L profiler first. Build knob: `-DMX_WOB_STEPS=3`; do not go below 3 with the shipped stiffness (unstable).
- With the lattice off, destruction is much weaker (random-shot win rate falls from ~8-10% to ~0%): hits no longer shear
  joints loose. So the DS fallback makes levels harder, not just uglier.
- Numbers (K, damping, break strain) were tuned by watching sway amplitudes and settle times, not against real-world values.
  Peak sway for normal hits is 0.15-0.9 cells (1-4 px on DS); big hits go further.
- Sounds (`CRACK`, `BOUNCE`) have never been heard. Dreamcast still has no sound at all.
- The Makefiles, libnds and KOS calls remain unbuilt / unverified, as before.

## DS build: what changed in the earlier pass
**Presentation** (screenshots in `docs/`)
- Both screens used. TOP = action camera (4 px/voxel, smooth scrolling, follows the man, lingers on
  explosions/evictions, screen shake, parallax sky + hills). BOTTOM = touch screen: while aiming it
  zooms to the sling at 4 px/voxel with a trajectory preview (stops where it would hit a wall) and a
  power bar; in flight it becomes a 2 px/voxel overview with a HUD strip and RESTART / MENU buttons.
- Generated 2/3/4 px material tiles with bevels, wood grain, brick joints, stone speckle, glass glints, TNT
  label, steel rivets, 3 damage states. Wallpapered interiors (rooms are exported by the core).
- Redrawn man (curvy dad bod, belly overhanging a waistband, briefs, tube socks, receding hair,
  proud mustache, towel cape in flight, goo coat + drips when stuck, different poses for aim/fly/dive/stuck)
  and landlords (top hat, monocle, briefcase, scared faces when the man is close).
- Particle types (debris, fire/smoke, sparks, goo, dust, stars, flying top hats), text popups
  ("EVICTED!", "NO RENT!", "BOOM!"), 3x5 bitmap font, title screen with level select, pause, win panel
  with 1-3 stars and a score count-up, lose panel, per-level best score (RAM only).
- Sound: sweeps/arpeggios instead of flat beeps, stereo pan from the event's x position, rate limits,
  graceful drop when the 6 PSG / 2 noise channels are busy, 32-step music loop (Y toggles).
- D-pad + A control scheme as an alternative to the stylus.

**Performance work (core, shared with Dreamcast)**
- ARM-mode build for the solver and blitters, solver hot paths in ITCM (`MX_HOT_ITCM`), libnds hardware
  divide/sqrt via `mulf32/divf32/sqrtf32` (`MX_LIBNDS_MATH`), adaptive physics substeps (2..3, chosen from
  measured frame cost via `cpuStartTiming`), optional support-pass interval, interior-voxel contact culling.
- On-device profiler: press **L** to show physics time (x0.1 ms), current substeps, and contact checks.
  Use it first on real hardware - none of the numbers below are DS timings.

**Core bugs found and fixed while doing this**
1. A slowly creeping rubble chunk could keep a level from ever ending (the lose check waited for all chunks
   to sleep; one never did). Sleep now relaxes with age, a second 1-second net-displacement test catches
   jittering stacks, and there is an 8-second safety net. Found by a new random-shot sweep.
2. In fixed point the 8 man substeps lost ~6% of time (`dt/8` truncation) and the chunk substeps ~3%. Slices now
   add up exactly to `dt`. Fixed-point dive shots now agree with float (22 -> 23 of 25 shots match closely).
3. Front-end: the first-cut DS code DMA'd from a cached RAM buffer without flushing the data cache. Now
   `DC_FlushRange` before each `dmaCopy`.

## Verified here (host machine only)
- `sh test/run_tests.sh` builds the core in float and fixed point; all 7 levels stand on their own;
  35 scripted shots run, float/fixed agree closely; **560 random shot sequences (280 per math mode) never hang**.
- Path with libnds hardware-math hooks (`-DMX_LIBNDS_MATH`, against a stub implementing the documented
  mulf32/divf32/sqrtf32 semantics) is bit-identical to the software path on all 35 shots.
- The REAL `ds/source/*.c` game loop runs on a stub libnds with scripted stylus input: title -> level select ->
  aim -> fling -> belly flop -> evictions -> win/lose panels, on levels 1, 3 and 5. Both screens were dumped and
  inspected (`docs/`). The sound stub enforces the 6 PSG / 2 noise limit: no overruns, peak PSG 4/6.
- Dreamcast front-end still builds and renders against the updated core.

## Honest numbers
- Interior-voxel contact culling only removes ~6% of contact checks on the shipped levels (34.8 -> 37.1
  checks/frame without it). It helps more on large intact chunks; it is not the big win I hoped for.
- Bit-identical core results before/after the speed hooks (float and fixed) - until the two fixes above,
  which intentionally change behaviour slightly.

## NOT verified (no toolchains / hardware available)
- `ds/Makefile` and `dreamcast/Makefile` have never been run. DS needs devkitPro (devkitARM + libnds);
  Dreamcast needs KallistiOS.
- No emulator or hardware run: **DS frame time is unknown**. The profiler (L) exists for this. First levers if it
  is slow: `-DMX_DS_MAX_SUBSTEPS=2`, `-DMX_SUPPORT_INTERVAL=2`, fewer particles (`NPART`), 30 fps physics.
- libnds / KOS calls were written from memory; the stubs only prove self-consistency. Most likely first-build
  nits: `soundSetFreq` / `soundPlayPSG` signatures, `BG_GFX_SUB` / `BG_PALETTE_SUB` / `bgInitSub` names,
  the `.itcm` section attribute, and ndstool rules in `ds_rules`.
- Sound has never been heard; the tune and effect timings are untested by ear.
- Music/SFX channel usage assumes libnds returns a negative id when no channel is free.
- Best scores are in RAM only (no FAT/save).

## Build
DS:         `cd ds && make`            -> mandex.nds  (melonDS / DeSmuME / flashcart)
Dreamcast:  `cd dreamcast && make`     -> mandex.elf  (Flycast / redream / selfboot)
Host tests: `sh test/run_tests.sh`     (also regenerates PPM screen dumps via `test/ds_render`)

## Controls
DS:  stylus - drag the man back, lift to fling; tap in flight = belly flop.
     D-pad moves the man in the sling, A flings / flops.  START pause / next / retry.  SELECT restart.
     Y music.  L profiler (on the title it acts on release).  B (when paused or after a level) back to the menu.
     Title: touch OUTFITS (or R) for outfits, touch ACH for achievements, L+R+SELECT = unlock every level (session only).
DC:  analog stick pulls the man back, A launches / belly flops; START next/retry; Y restart.

## Next steps (suggested order)
1. Build both Makefiles; run in melonDS and Flycast; fix API nits.
2. DS: read the profiler on a real console/emulator; tune substeps, particles, `MX_SUPPORT_INTERVAL`.
3. DC: bring it up to the DS front-end's level (text, HUD, sound via AICA from the same event list, sprites).
4. DC 3D upgrade (64x32x24 voxels, instanced cubes, 3D support flood fill) - core stays 2D until then.
5. Saves: DS FAT/SRAM for best scores; DC VMU.

## TITLE SCREEN pass (HTML + DS + Dreamcast)
- The HTML title is drawn by `drawSlamScene` / `drawSlam` / `drawLandlordScared` in `web/index.html`.  `drawSlam` poses him as a small real 3D pose (joints
  in his own space, turned about 29 degrees to the camera so we see his back and rear from the right, mild perspective, every limb a projected tapered capsule
  and every mass a projected ellipsoid, drawn far to near): butt down, legs up in a V with socks and shoes, hands on his shins, the towel streaming up behind him.
- `python3 tools/gen_title.py` renders `drawTitlePicture` 4x too big, box-filters it down, quantises it (DS 190 colours, Dreamcast 96) and writes `art/title.h`.
  DS: RLE, unpacked once into a 48 KB buffer, memcpy'd to the top screen each frame; the text colours (palette slots < 64) are untouched.
  Dreamcast: ~9.6k flat quads (horizontal runs merged with identical runs below), drawn at 2x; PRESS START is ~460 more.
- Not run on hardware or in an emulator: the DS/Dreamcast title code is checked only by the host harnesses (`docs/ds_1_title.png`, `docs/dc_title.png`) and the CI compile.


Title pan: both consoles keep the whole 16:9 title picture and slowly pan right to the man and back (hold, glide, hold, glide; 560 frames), so the full landscape is seen on the small screens. Shots: docs/title_pan.png (top DS, bottom DC). Not run on real hardware or an emulator.


OOPS gag (consoles): the rare version of the cape taunt, as in the HTML (1 in MI_OOPS_ODDS = 5).  tools/gen_manspr.py now also draws OOPS_A..H
(boxers slip, wobble, EEK, cover up, SNAP, WHEW) into art/manspr.h; art/manidle.h has GAG_OOPS (only ever picked as a cape taunt) and the new fx
FX_EEK / FX_SNAP / FX_WHEW.  DS: two new tone effects (SFX_EEK, SFX_WHEW) plus EEK! / SNAP! popups, SNAP reuses the slap + boing sounds.  DC: sweat / spark
particles.  Shots: docs/ds_oops_sheet.png, docs/dc_oops_sheet.png.  Run on a desktop test setup only; no real DS / Dreamcast / emulator, sounds never heard.


GRAPHICS pass (both consoles).
Dreamcast: no more flat quads.  tools/gen_dc_art.py bakes textures from the HTML's own drawing into art/dcart.h (run-length coded 16 bit pixels): a sky (RGB565), a
mountains / city / hills / ground layer with the sling post (ARGB1555), an atlas (block tiles for every material x variant x damage, landlord, cloud, soft puff, stars, head icon,
font), every man frame smooth at 1.6x the drawn size (ARGB4444, no towel: the cloth is still simulated), and an 852x480 title picture (RGB565).  dreamcast/main.c draws textured
quads in the translucent list, keyed by z; the man is a small mesh when he jiggles; the HUD, banners, hint and end panels use the baked font.  Texture memory is about 3.8 MB
(the title picture is freed when the game starts).  The real KallistiOS build in CI compiles it and links it (verified from 869e70d on; before that the CI step was dying before make ran); nothing has been run on a Dreamcast or an emulator, and test/dc_render.c has a
small software rasteriser for the PVR textures so the screens can be photographed on a host.  The old flat-rect title data in art/title.h (TITLE_DC_*) is no longer used.
DS: the procedural backdrop gained snow-capped mountains, a city skyline, trees and rolling hills (palette slots 110..122); the ground gained grass blades, flowers and the fence.
Screens: docs/dc_*.png, docs/ds_3_eviction_L5.png, docs/dc_art_sheet.png.

## DS: "hardware" build (current)
The DS ROM is built without libnds / calico (`ds/bare`, `tools/build_ds_bare.sh`): calico's start-up code did not boot in DraStic
(white screen) and crashed melonDS, while ROMs with their own crt0 (the user's godguy_hw / wordPass / bore2) run.  Layout copied from
those: ARM9 image at 0x02000000 with a 0x800-byte secure-area filler, entry 0x02000800, ARM7 at 0x02380000, plain DS header (unit code 0),
`ndstool -h 0x4000`.  `ds/bare/nds.h` is a small stand-in for <nds.h>; `hw9.c` does video / keys / timers / divider / sqrt / sound
commands; `arm7.c` polls X/Y, the pen (SPI touch with firmware calibration) and plays the sound ring buffer.  Caches are not enabled,
there is no ITCM use.  NOT verified on any emulator or hardware.  `tools/ntr_patch.py` fills the secure area and fixes the CRCs.

## Nintendo 3DS build (3ds/)

Homebrew `.3dsx` (devkitARM + libctru, no citro2d/citro3d): the shared core in float mode, drawn in software straight into the two framebuffers.
Top screen = the whole level at 3 px per voxel; bottom screen = the sling at 4 px per voxel (touch to drag the man back, lift to fling, tap in flight to belly flop).
D-pad / circle pad + A also work. No sound and no save yet; the man is still drawn as circles and rectangles.  Outfits cycle on the title (L/R) and achievements show as a toast only, with no screens for either; the end-panel stars, level locks and best-score line are still to do (see ROADMAP PROGRESS). CI job `n3ds` uploads the artifact `mandex-3ds` (mandex.3dsx + mandex.elf).
Nothing has been run on a real 3DS or in an emulator (Citra / Lime3DS): it is only known to compile.

## Uniform controls + 3DS stereo 3D

| Action | DS | 3DS | Dreamcast |
|---|---|---|---|
| Aim | touch drag or D-pad | touch drag, D-pad or circle pad | analog stick |
| Release / belly flop | A (touch to dive too) | A, B or touch | A or B |
| Power-up cycle | X | X | X |
| Pause / resume | START | START | START |
| Restart level | SELECT | SELECT | Y (no SELECT on the pad) |
| Title: level | D-pad left/right | D-pad left/right | D-pad left/right |
| Title: outfit | R cycles, or touch OUTFITS for the picker (L is the profiler) | L / R | left / right trigger |
| Title: achievements | touch the ACH counter | no screen yet | no screen yet |
| Title: unlock-all cheat | L+R+SELECT | not yet | not yet |
| Play | A or START | A, START or touch | A or START |
| End screen: back to title | B | B | not available (title art is freed to save VRAM) |

3DS 3D: the top screen is drawn once per eye with per-layer horizontal disparity scaled by the 3D slider
(sky furthest, then back faces of blocks, the building, landlords, sling and valve, aim dots, the man and
debris nearest). Slider at zero draws one flat image. This is a parallax effect, not real geometry,
and has not been seen on a real 3DS or in an emulator.

## TURNING IDLES pass (HTML only)
The browser idle gags now run on the 3D turn rig (`drawSpin`) instead of flat front poses: real head yaw / pitch / roll (`ld`, `hp`, `hr`),
body yaw with planted feet and lagging hips (`th`, `plant`), free 3D arms (`arms`), the cape hanging in 3D and swinging after the turn,
gaze, and 11 face expressions (`FACE3`).  Key poses are `T_*` entries of `SPIN_POSE`; `spinLive` glides between them.  Gags: look around + double take,
belly slap in 3/4 view, butt scratch + sniff, armpit sniff, brow wipe, belly-button lint, nose pick, bicep kiss, flex show, burp, yawn; fart and butt wiggle
now step-turn round first.  The console sprites (`art/manspr.h`, `art/manidle.h`) are unchanged and would need `tools/gen_manspr.py` support for the `T_*` frames.


## PORT 13 step A: post-run crumble (core, all front-ends; DS popup)
- After every real end of a run (he stuck and popped, or flew off the map) the damaged walls crumble, as in the browser's `aftershock()`.  Core: `dmg[]` marks the four
  neighbours of every destroyed block; `aftershock()` opens a zone (Manhattan <= 3 in a 5x5 window) around each damaged block or snapped joint, clears the anchor of zone blocks,
  and snaps all four joints (both sides) of the weakest 80 blocks below 70% health (sand excluded; integer maths, counting buckets, no sort, no malloc).  `end_run()` calls it and
  emits `EV_CRUMBLE` (x,y = centre, a = how many).  Bursting on the air valve is not a run, so it does not crumble.  `dz_lo/dz_hi` bound the scan to the rows that matter.
- Also: when the support pass has no free chunk it now retries 20 frames later (`retry_cd`) instead of leaving a loose slab hanging in the air for good.
- DS: `CRUMBLE!` popup, dust, thud + crack sound, camera lingers.  3DS and Dreamcast ignore the new event for now.
- Cost: +7,200 bytes in `MxWorld`.  Plow hits alone rarely crumble anything (neighbours only lose 18%); blasts and chunk impacts do.
- Tests: `test/crumble_test.c` (float + fixed).  Win rates and the 35 scripted shots will differ from before: re-baseline.
- NOT verified: no DS or emulator run; sweep and scripted-shot re-baseline not recorded yet.
- Still to do for port 13: B collapsible buildings (`MX_MAX_CHUNKS` stays 16 on the DS), C options screen + save blob v2.

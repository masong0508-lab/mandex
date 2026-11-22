# Wolfman street encounters — November 22, 2026

## Gameplay and new content

The original Wolfman chapter generated two or three houses using several
parameter changes, leaving a long and often empty street between the first
pair. All 20 levels now get a **small fourth tactical space** in that gap.

| Wolfman level index (0-based within chapter) | New encounter | Player strategy |
| --- | --- | --- |
| 0, 4, 8, 12, 16 | Glass tollhouse | Clear the thrower or detonate the lower charge |
| 1, 5, 9, 13, 17 | Rubber customs kiosk | Rebound around rubber, or open the wooden roof; later patrols sprint |
| 2, 6, 10, 14, 18 | Sand hopper | Detonate the second-floor pin to release the sand reservoir over the ground guard |
| 3, 7, 11, 15, 19 | Stone-capped tax bunker | Work around the stone top and floor charge; later guards carry shields |

The structure is centered in the gap between house 1 and house 2. The
minimum gap is 12 cells, and the landmark's eight-cell width leaves at
least two cells between the original walls. Its walls, floors, roof, TNT
and one extra landlord are identical across web and console builds.

The 60 existing level IDs remain stable (indices 40–59 for chapter 3),
and the save format, unlock progress and achievements are untouched.
The browser supplies a player-facing hint for each variation without
overriding the existing boss, shield and healer tutorial hints.

## Cost and compatibility

This is level-construction work, not a new tick-time simulation mechanic.
It reuses the existing house, TNT, sand and landlord logic and renderers;
it introduces no new dynamic arrays, textures or per-frame scanning.
Each generated level has two additional room records and one landlord.
The existing `MX_MAX_TARGETS` and `MX_MAX_ROOMS` bounds are tested.

All three console ports share `core/mx.c`; the browser mirrors the
geometry in `web/index.html`. Future gameplay tweaks should modify both
in lockstep.

## Validation

- `cd test && gcc -std=c99 -O2 -Wall -I../core ../core/mx.c wolf_annex_test.c -lm -o /tmp/wolf_streets && /tmp/wolf_streets`
- Repeat with `-DMX_FIXED` for DS fixed-point math.
- `sh test/run_tests.sh` runs both modes, plus all existing regression checks.
- With Playwright installed, `PARITY=1 sh test/run_tests.sh` compares 0–38
  and 40–59 against actual loaded browser worlds, cell by cell (Landlord HQ
  is intentionally excluded because its web world is three times wider).
- A new GitHub Actions packaging run should verify the DS, 3DS and
  Dreamcast builds and browser packaging on the latest commit.

The level checks verify three seconds of idle integrity, not successful
hardware playthroughs. Original DS, 3DS and Dreamcast sustained FPS and
worst-case destruction performance still require emulator/device tests.

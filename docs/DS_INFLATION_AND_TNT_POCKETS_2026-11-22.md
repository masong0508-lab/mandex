# DS inflation art and TNT impact pockets (2026-11-22)

## DS character rendering
The previous inflation painter grew a largely plain skin ellipse to **112px wide at ppc=4**, hiding the character frame and stepping in coarse 4px jumps. `spr_man_inflation` caps the body to a **56px silhouette plus one-pixel rim**, follows real pressure in single-pixel steps, and adds shaped shadows, highlights, contour, navel, pressure creases and clothing waistband. The original face, head, cloth and pose are kept. The existing sprite palette is reused so Wolfman skin and human skin match their costumes.

Only DS visual presentation changes; the core retains its **3.6-second inflate/pop mechanic**, collision radius, and speed. No framebuffers, sprite atlas, heap allocations or float operations added.

The DS rubble renderer now flashes a tiny fuse cap on the active charge's actual transformed voxel coordinates, rather than at the parent rubble centroid. Other ports continue to use their previous indicators.

## Expanded Wolfman encounters — shared across C and browser
Rubber kiosks at chapter 3 indices 1, 5, 9, 13 and 17 have an upper **rubber spring shelf** (x=annex+2..5, row 51) and lower rubber landing bed (row 59). Sand hoppers at indices 2, 6, 10, 14 and 18 have a **sand landing bed** (same x, row 59). Materials use the existing impact damping behavior: soft falls are more likely to leave TNT intact, but explosions and violent collisions can still trigger it. They add ten gameplay situations without a new level ID, extra NPC, world storage or simulation cost.

All layout coordinates and descriptions are mirrored in `core/mx.c` and `web/index.html`; `test/wolf_annex_test.c` checks them in fixed/float modes.

## Verification
- `test/ds_inflation_detail_test.c`: the production DS renderer at 2/3/4px-per-cell, 0–100 pressure stages, both facings and screen edges, with palette detail and overflow canaries.
- `test/wolf_annex_test.c`: exact landing bed geometry and 3 seconds of initial world stability for each of the 20 Wolfman levels.
- `test/mobile_tnt_test.c`: real impact material damping, delayed fuse and precise explosion origin.
- `Build Nintendo DS` and `Host tests` in CI check compilation and regression behavior. Native hardware FPS and hands-on playability still require a physical-device playtest.

The landing pads are exactly inside the 60-row physics grid: floor 59 and first upper landing 51. CI validates occupied materials at those locations, not just placement calls.

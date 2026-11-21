# Eviction Objective Tracker — coherent gameplay UI

## Why
**WRECKED %** is a destruction/score metric, not a victory requirement. The level actually advances when every landlord is evicted, after a 1.6 second settling period. Landlord HQ adds two intermediate district transitions. A player could otherwise demolish nearly everything and have no idea why the level was still active.

## Implemented
- New stateless, read-only `core/mx_objective.h` returns the actual remaining/total landlord count, objective phase, and direction of the last surviving target relative to the player's world position. Phases: EVICT LANDLORDS, LAST LANDLORD, SECURING AREA (the post-eviction win delay), NEXT DISTRICT (Landlord HQ), EVICTION COMPLETE, OUT OF MEN, NO MEN LEFT.
- **Nintendo DS:** a compact outlined mission caption below the top-screen HUD, with count or final-target arrow.
- **Nintendo 3DS:** the same label in a readable strip alongside (not replacing) the existing WRECKED meter and bottom HUD, visible in both stereo views.
- **Dreamcast:** a grounded label below the level HUD, with the objective count, final-target direction, and district-transition status. The existing HUD's target count reuses the new snapshot rather than scanning twice.
- **Web, landscape and portrait:** the same objective status, in a narrow legible overlay below the header; the final target arrow responds to player position. The browser reuses one small persistent `OBJECTIVE_UI` record rather than creating a new object every render.
- No new assets, physics changes, save fields, heap allocations or new framebuffer. Counts scan only the existing tiny target array when the HUD renders; not the 7,200-cell voxel grid. Victory is never inferred from WRECKED %.

## Tests
- `test/objective_tracker_test.c`, compiled in both fixed and floating-point modes: 3→1→0 evictions, direction left/right/aligned, empty lives, 1.6-second staging, all HQ district stages, final win/loss.
- `test/browser_objective_test.js`: parity for those cases, mobile/desktop HUD placement, actual browser JavaScript parsing.
- Both are part of `test/run_tests.sh`. Console compilation and screenshot review must confirm the new status fits without obscuring action.
- In-game guidance is read-only: a last-target direction is only displayed when exactly one living landlord remains; location accuracy is based on logical world x, not the camera center.

## Last-landlord world locator

When exactly one landlord survives, the game now displays a small **LAST**
label over that character in the actual scene, on DS, 3DS (each stereo
projection), Dreamcast and the web. It is deliberately suppressed while
multiple landlords remain. The rare collapse-survivor gold exclamation
marker retains priority over the pink LAST marker on the consoles to avoid
stacking competing tags; the HUD still reports LAST LANDLORD.

The marker draws with existing small font primitives. It adds no actor,
texture, frame state or particle allocation. Every renderer computes the
one-target condition once before the landlord loop rather than rescanning
the target list per actor. The standard world culling margins remain valid
because the marker fits within the existing warning and survivor tells.

## HUD layout and real-pixel regression

The browser portrait streak originally started at y=67, overlapping the
objective caption (y=66..94). The streak now begins at y=100 in portrait,
remaining below the caption even on narrow 320–699px displays; desktop
keeps the existing unobtrusive corner layout. The browser regression checks
rectangular non-overlap at seven viewport widths.

The first full host run exposed a test-isolation problem: the rubble-only
canvas VM cut the production code immediately before the landlord loop,
accidentally including the new last-target objective expression. That VM
has no game state by design. Its source slice now correctly ends at the
start of final-target marker setup, leaving the actual rubble-draw code
under the same pixel-order and culling assertions.

A new `DS_OBJECTIVE_PROBE` in `test/ds_render.c` executes the real
Nintendo DS font and world-sprite renderers at 256×192 and checks distinct
pixel hashes for 2 landlords, last landlord, securing area, and an actual
on-character locator with the second landlord offscreen. It runs in the
regular host suite; screenshots/emulator checks remain additional gates.

## Four-way offscreen objective locator

When just one landlord survives, the camera now shows a small pink **LAST** cue at the nearest appropriate screen edge whenever the actor is outside the usable view. This distinguishes a genuinely offscreen landlord from a visible one and prevents the player from needing to pan randomly through rubble. Left/right/up/down directions use an allocation-free, read-only `mx_objective_edge()` shared by Nintendo DS, 3DS and Dreamcast; the browser uses an equivalent reusable object and the existing world-to-screen camera transform.

The cues deliberately reserve the game's HUD/control strips: DS top 64px/bottom 12px; 3DS top 54px/bottom 30px; Dreamcast top 74px/bottom 34px; browser top 76px in landscape or 138px in portrait, bottom 56px. Existing on-character LAST/survivor markers are unchanged. The entire feature is dormant for two or more targets, when the game is completed, or when the final landlord is already visible in the playable area. No textures, new world entities, dynamic arrays, framebuffer or new save fields. Core and browser tests cover all four directions, diagonals, visible suppression, narrow-screen suppression and target-index invalidation; the real DS renderer's pixel-hash probe verifies cue display only when the final actor is offscreen.

The enlarged DS culling margin for the LAST tag is required: its glyph starts 9 world cells above the actor and remains visible near the lower viewport edge after the actor has left the picture. The cull-reference test tracks exactly the same conservative bound, preserving pixel parity against the unculled renderer.

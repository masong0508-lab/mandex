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

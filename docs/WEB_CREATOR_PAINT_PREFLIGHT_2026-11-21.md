# Mandex browser creator — large brush performance and build confidence

## Player-facing improvements

- **CHECK BUILD** is now available in both landscape creator actions and the portrait LEVEL ACTIONS screen. It calls the same canonical level validator used by Save and Test Play and reports either an actionable error (missing structural cells, invalid landlords, bad shot allowance, etc.) or **BUILD READY** with actual landlord and shot counts. Validation runs *only when requested*, not every frame.
- Rectangle previews are transactional: a second-finger interruption, orientation change or cancelled pointer capture cannot commit a half-finished rectangle, consume Undo history or discard a valid Redo sequence. A normal release makes exactly one reversible edit.
- Tapping outside the playable 120 × 60 grid, attempting to add a landlord on solid material, or exceeding 12 landlords does not waste an Undo snapshot.
- Painting a large brush/rectangle no longer creates a new landlord-target array for every touched material cell. Removal is in place; it still deletes only targets on painted cells, retaining target order.

## Performance and engineering

The previous per-tile `E.t.filter(...)` allocated a new array for **every painted tile**, even with no targets on the grid. It is replaced by a reverse in-place scan of the at-most-12 existing targets and `splice` only for actual overlap. Full-width rectangles can paint thousands of material cells without thousands of temporary arrays. This is an allocation reduction by code inspection, **not** a measured frame-rate claim.

The rectangle snapshot is deferred until pointer release. The existing 24-state history and source MX1 save encoding are unchanged. No new resident grid, canvas, texture, animation frames, level data, console resources or per-frame validation pass.

## Regression and release gates

`node test/browser_creator_paint_test.js` executes the shipped production editor functions and covers rect commit/cancel, Redo preservation, target-array identity/removal, out-of-bounds taps, full/blocked landlord placement, valid/invalid preflight and both UIs. `.github/workflows/browser-editor-paint.yml` runs it alongside the creator codec, starters, pointer and responsive layout regressions. It is also in the full `test/run_tests.sh` suite.

Verify actual latest-head GitHub Actions and perform portrait/landscape touchscreen checks before claiming a completed release; DS/3DS/Dreamcast core and render hot paths are unchanged.

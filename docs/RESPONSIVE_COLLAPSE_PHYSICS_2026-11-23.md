# Responsive Mandex structural collapse and rubble dynamics
November 23, 2026

## Addressed

1. The shared C physics could leave disconnected components larger than
   512 voxels floating forever when a single fixed-size rigid body was too
   small. Both native and browser engines now detach the first 512 connected
   voxels and continue on subsequent structural-support passes until the
   remaining unsupported cells have fallen. The cap preserves the original
   Nintendo DS chunk-cell storage budget.
2. Voxel damage below the fatal threshold formerly left spring joints as
   strong as a fresh wall. Both engines now reduce permitted joint strain
   to 85% at or below half HP, and 70% at or below one-quarter HP, while
   retaining the original healthy-material strength.
3. Strong moving-rubble contacts now wake sleeping bodies as part of the
   existing contact solver when relative speed exceeds 5 cells/second.
   A weaker contact remains quiet to avoid permanent jitter.

## Physics and performance safeguards

Detached partial chunks expose all contact edges at split boundaries so
they are collidable. Ordinary components keep sparse edge marking.
No extra C voxel buffers, heap allocations, framebuffers, save formats,
sprites or particle pools were added. The browser builds on the previously
implemented reusable support traversal and collision hash. Splitting a
previously inert giant piece can legitimately increase active body and
collision work, which requires profiling in worst-case scenes.

## Regression and acceptance

- test/collapse_response_test.c exercises real shared C support detachment,
  joint fracture and moving-versus-sleeping contacts in float and fixed.
- test/browser_collapse_response_test.js extracts real browser source,
  including 120- and 360-column worlds, 512-cell splits and wake rules.
- .github/workflows/responsive-collapse.yml runs the focused checks.
- test/run_tests.sh includes both regressions.

Passing automated regression gates is not equivalent to verified original
hardware FPS, successful full-game playthrough, or final ROM acceptance.

# Wolfman night alleys — second stage-structure pass

## Content

The final twelve Wolfman levels (49–60) contain three main houses.
A four-cell-wide micro-structure now fills part of the second gap between
houses two and three, which is at least six cells wide in every stage.
The original tactical building between houses one and two stays intact.

| Pattern | Mini-encounter | Interactive behavior |
| --- | --- | --- |
| 0 | Glass signal booth | Glass shell and roof break to expose a TNT keg |
| 1 | Rubber spring gate | Rubber cap and floor cushion an interior TNT charge |
| 2 | Sandbag spill station | Glass shell holds a stack of sand against a TNT weak point |
| 3 | Steel gate | Armored walls protect a charge under a fragile glass roof |

The same coordinates and materials are used in `core/mx.c` and
`web/index.html`. There is no new projectile, enemy, image atlas,
framebuffer, malloc, persistent game state or per-frame simulation loop.
The additional room remains within `MX_MAX_ROOMS=20`, and
`MX_MAX_TARGETS=20` is unchanged. All 60 stage IDs are preserved.

## User guidance

`mx_level_tactic` in the shared C core supplies a ROM-resident string
for each recurring encounter and the key Wolfman enemy milestones
(health bars, shields, healers and boss double throws).

- The Nintendo DS uses the existing level-entry banner.
- Nintendo 3DS draws one small tactical panel only while the level
  introduction timer is active. It is *not* drawn throughout the level.
- Dreamcast replaces its generic banner instruction with the tactic.
- Browser retains existing milestone tutorials and gains four matching
  level-tip descriptions for the second alley.

## Validation

- `test/wolf_annex_test.c` tests all twenty Wolfman maps in float and
  fixed C, including exact block positions for the twelve second-alley
  scenes, pool limits, and 180 physics steps after level entry.
- `test/browser_wolf_alley_test.js` executes the actual browser
  chapter-three constructor in a harness, checking the same twelve
  structures, TNT charge positions, materials, tips and counts.
- `wolfman-night-alley-gates.yml` and the existing all-port workflows
  verify these additions in CI.

Emulator smoke and builds are not evidence of physical handheld FPS.
A full manual gameplay playthrough of the new structures is still required.

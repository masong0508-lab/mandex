# Mandex — counterplay to thrown briefcases

## Player-facing behavior

Thrower and boss briefcases are no longer purely unavoidable when the player is on foot. A **timely forward punch** can intercept an incoming briefcase in the existing punch hitbox; a **descending belly slam** can also smash one. Every actual interception:

- Destroys that one active briefcase before its next collision update
- Awards a modest **+150 points**, once per case
- Produces the existing impact sparks and sounds with a **PARRY! +150** indicator on the browser, DS and 3DS. Dreamcast reuses the existing impact particles and audible BONK cue and shows score via the standard HUD

Behind-the-back punches, out-of-height and out-of-range cases do not get intercepted. A briefcase still hits an undefended player normally. The punch's existing 0.3-second cooldown and 0.2-second animation remain unchanged, as do the boss's windups and ordinary level objectives.

## Engineering

The shared C core checks the **four existing MxCase slots** while processing an actual walk hit, not on every physics step. It deactivates a caught case, issues an existing **EV_BONK** with `m=2` (0/1 retain bat/shield swat semantics), awards the fixed 150-point bonus and reports a small positive hit impact. This adds **no new world state, heap allocations, arrays, framebuffers, sprite sheets or projectile capacity**.

The browser uses an equivalent radius margin of 6 pixels (0.75 world cell), removes cases in reverse index order from its existing array, and uses the already available popup/sparks. Both punch and slam share the walk-hit collision path; its inactive flag prevents duplicate scores when a slam checks two overlapping hitboxes.

The feature is deliberately defensive and skill-based, not automatic deflection. It does not change airborne launch collision or damage while the player is not attacking.

## Tests

- `test/case_parry_test.c`, float and fixed: actual public update, punch, multi-case contact, misses, exact bounds, slam two-hitbox deduplication, no repeat scoring, non-parry damage, event flag integrity and normal empty punches.
- `test/browser_case_parry_test.js`: executes the shipped walking attack functions and projectile update; mirrors the native cases and validates browser parse.
- `test/browser_input_smoke.py`: actual Chromium keydown/keyup, portrait visual capture of **PARRY! +150** and an identically prepared unguarded briefcase that must still hurt the player.
- New `.github/workflows/briefcase-parry.yml` runs the focused tests, and `test/run_tests.sh` includes them in the long float/fixed/console host suite.
- Existing DS/3DS/Dreamcast compilation, DS boot tests, cross-port pixel parity and packaging remain required. Passing host tests does not establish physical-device FPS or complete hardware gameplay.

## Remaining acceptance

Review Chromium screenshots of successful and failed defense and actual UI legibility. Confirm sound cue and event rendering in native emulators. Compare console ELF writable memory and validate release-ROM builds. The prior release should remain available while CI is pending.

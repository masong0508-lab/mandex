# Walking responsiveness: short hops and edge forgiveness

## Gameplay

This milestone extends the Chapter 2/3 walking sections on **browser, DS, 3DS and Dreamcast** without changing launch physics, rubble destruction, existing levels, enemy AI or saved progression.

- **Tap JUMP** for a low, controllable hop. Releasing within 0.16 s caps the remaining *upward* jump speed at 22.5 world cells/s (180 browser pixels/s). The initial launch impulse remains −43.125 cells/s.
- **Hold JUMP** for the original full-height trajectory. The same gravity and launch strength are retained; the timer only limits how long an early release can trim the jump.
- **Ledge grace:** leaving a platform keeps a jump available for at most 0.10 seconds. Any grounded/coyote jump consumes the grace immediately, so a second airborne jump is impossible.
- **Special moves:** stomps replace the jump and cancel the hold window. Rubber pads retain their ordinary and boosted bounce speeds (−58.75/−67.5 cells/s), regardless of whether the JUMP button is held. Early jump release cannot shorten a pad bounce.

All behavior uses two scalar fields in the existing walking-man state, **not** new arrays, textures, screen buffers, per-frame physics passes, or heap allocations. Browser uses the same simple timer properties. This is 8 additional bytes of man-state in the native float/fixed build, subject to target ABI padding; runtime and static section sizes must be audited from the actual builds.

## Verification

- `test/walk_jump_forgiveness_test.c` calls the production `walk_step` function in both float and fixed-point modes. Covers low/high apex, release cap, live step-off ledge, active and expired grace, no second jump, stomp, and unchanged bounce.
- `test/browser_walk_jump_forgiveness_test.js` extracts the shipped browser walking solver and actual collision helpers. Covers the same gameplay rules plus JavaScript parse and clean Enter Walk initialization.
- `test/browser_pad_stomp_test.js` retains full state/effect parity against its frozen reference for **ordinary walking, punching and pad launches**. Deliberately modified short-hop and coyote actions are verified independently, rather than falsely asserting old jump physics.
- The fast `.github/workflows/walk-jump-controls.yml` and full `test/run_tests.sh` execute the relevant regressions. Browser viewport screenshots and native console builds remain required before merge.

The higher jump is not stronger than before, and the new grace period is intentionally modest. A passing host suite is not a claim of physical-device FPS, memory headroom or complete ROM playthroughs.

## Acceptance steps

1. Run float and fixed native fixtures, web fixtures, existing pad and AI tests.
2. Build all console ROMs and inspect merge-head static-memory sections.
3. Run actual browser keyboard/touch smoke tests and review portrait walking HUD.
4. Verify DS emulator boot, packaging, and no adverse effects on chapter-two completion.

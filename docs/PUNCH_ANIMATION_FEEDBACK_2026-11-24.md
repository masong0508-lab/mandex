# Attack-facing punch feedback — November 24, 2026

## Corrected behavior

- DS, 3DS, Dreamcast and browser now draw the punch sweep on the live gameplay-facing side. A quick reverse-and-punch no longer shows the effect behind the player while the rear-turn animation is still completing.
- The effect follows the actual attack rectangle: centre 2.125 cells forward and 0.375 cells above the player, half extents 1.625 by 2 cells. The native damage and active parry paths use the same shared constants, with their existing projectile padding.
- The 0.2-second attack is visible throughout its original active window. The visual arc shortens from seven to five to three points as the punch recovers. On browser/Dreamcast the effect fades with opacity; handhelds use the existing palette or framebuffer colors.
- The visual sweep is distinct from the body-turn and lunge transforms. The browser renders it after restoring the character transform. No new atlas, framebuffer, save field, heap allocation or physics timestep is needed.

## Verification

- `test/visual_feedback_test.c` checks both numeric modes, every turning tick and both faces, the three recovery stages, range, vertical bounds, and read-only helper behavior.
- `test/walk_turn_interrupt_test.c` compares the visible tip to real punch events through countersteering scenes.
- `test/browser_punch_fx_test.js` executes production punch and paint functions in grounded and airborne turning states, compares each drawn sample against the real hit rectangle, checks world-space transform and opacity restoration, and confirms all three native painters use the common helpers.
- The dedicated walking-animation gate and full host suite include these regressions. No original-hardware FPS or playthrough claim is made.

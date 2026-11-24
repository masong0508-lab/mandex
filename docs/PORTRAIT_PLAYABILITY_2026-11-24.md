# Portrait gameplay and touch controls — November 24, 2026

## Corrections
- A captured aim or soda-chug gesture belongs to exactly one pointer. Other
  fingers cannot alter, cancel or prematurely release that shot. Pointer
  cancellation, loss of capture, rotation, blur and relevant viewport changes
  reset input without firing. The previous valve seat is restored on a
  cancelled valve pull; otherwise the character returns to the sling.
- Portrait aim pickup is a bounded screen-space radius (64 virtual pixels).
  Movement uses the player's initial position and the captured zoom, not the
  original off-centre finger or the current camera scale.
- Walking pointers can slide off a control into a neutral gap and re-enter
  without lifting. Multiple thumbs holding a shared key retain independent
  ownership. Menu swipes keep the first finger as their sole owner.
- The active sling is accessible immediately at portrait district entry.
  Directional walking look-ahead gives more space ahead; fast flight/fall
  camera tracking protects the actor centre within the usable view.
- Larger 60px portrait action buttons for tools, sling selection and the
  active flight move. A confirmed two-tap END RUN control fills the gap
  between the five existing walking controls, so a keyboard is no longer
  necessary to give up a walking attempt.
- Last-target edge cues reserve the walking HUD or enlarged aim/action bar
  and the combined TNT/streak warning; the actual visual viewport limits
  CSS canvas dimensions during soft-keyboard/browser chrome changes.

## Acceptance
Production-source Node regression exercises real event handlers, actual
camera and HUD routines on 720, 960 and 1240 virtual-pixel portrait heights,
including two-finger releases, cancellation, valve seat restore, soda owner,
neutral slide/re-entry, wide finale sling, rapid tracking and action callbacks.
Existing responsive, combat, editor and Chromium input/render gates remain
in place. Normal landscape walking controls and physics rules are unchanged.
Original-device input testing and frame-rate measurement are not claimed.

# Portrait inspection and aiming feedback

- Inspection is a separate, explicitly selected mobile camera mode while the
  player is safely waiting to aim. Entering it focuses live targets in the
  active district; simulation continues normally.
- Drag pans a captured view pointer; +/- zoom around the usable viewport
  centre between 55% and 180%; Find Targets recentres the same district.
  Back to Aim restores the original automatic sling camera immediately.
- The manual camera never launches, alters an aim pull, activates a power,
  changes actor/target coordinates, changes inventory or spends a shot.
  Foreign pointer events cannot steal the pan. Capture loss releases only
  the camera pointer. Rotation, viewport resize, blur, a new shot or a
  changed district exit the manual view safely.
- Aiming gains a persistent 0–100% pull-strength meter and a large Inspect
  button, with separate readiness and tool hint space. The last-target
  indicator reserves the enlarged aim controls. Walking and landscape
  controls retain their previous geometry.
- Production-source regression covers normal and 360-column maps at three
  portrait heights, including camera bounds, action callbacks, ownership,
  state invariance and returning to the sling. Existing browser and ROM
  workflows remain required. Physical-phone playthrough and FPS unverified.

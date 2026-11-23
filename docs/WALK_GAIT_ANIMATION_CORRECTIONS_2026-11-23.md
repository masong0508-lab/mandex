# Walking gait and animation correction — November 23, 2026

## Problem

The standalone browser walking painter derived the stride from absolute map
X (`m.x / 15`), not actual horizontal travel. This shifted foot placement
depending on where a level loaded, made browser strides much longer than the
four-cell console gait, and risked advancing a pose after a character was
moved to safety from falling rubble. The painter also copied a new pose
object with `Object.assign` on every draw.

## Changes

- Record horizontal distance at the beginning of the walking simulation
  *after* the existing rubble escape/reposition logic; accumulate only
  collision-resolved distance when grounded.
- Use a repeating four-cell (32 screen-pixel world-unit) stride, matching
  the shared console engine used by Nintendo DS, Nintendo 3DS and Dreamcast.
- Blend the amount of stepping with actual horizontal speed, while suspending
  grounded stepping during a rear-facing turn, a jump or a fall.
- Reuse a single painting pose, preserving the existing head-leading angle,
  torso orientation, outfit and turn animation. No new sprites, textures,
  framebuffers or per-frame allocations.
- Preserve the original hitboxes, movement forces, level data and save format.

## Verification

`test/browser_walk_gait_test.js` executes the production walking and visual
step functions, checking four gait phases at distant world coordinates, a
real collision-locked wall, jumping, reversing direction, stationary poses,
and pose scratch reuse. A dedicated workflow also exercises the existing
browser turnaround and both native numeric modes.

This is an animation correctness and GC-allocation fix. No measured browser
FPS gain or hardware playthrough is claimed.

## Flight and tube animation parity

- The browser now mirrors the actual side and tumble silhouettes when flying
  left, matching the shared DS/3DS/Dreamcast `MxMan.face` behavior.
- The flight pitch uses `atan2(vy, abs(vx))`: left and right shots with
  equal trajectories have equal forward-relative pitch, without a sudden
  180-degree rotation on launch.
- A 12-pixel/s facing deadzone preserves the previous left/right side near
  a horizontal stall, avoiding a flickering pose in midair.
- Tube transport resets the incoming pitch and chooses the outgoing facing
  from the nozzle direction before easing toward its new trajectory.
- The browser dive spin mirrors with the drawn side, keeping the rotation
  consistent with the three console frontends.

`test/browser_flight_pose_test.js` exercises launch, tube and actual drawing
code for both directions, including rotation and scale signs. None of these
cosmetic changes affect collision velocity, damage, levels or saves.

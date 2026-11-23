# Mandex: live TNT status and quality-matched inflation artwork

## User-facing improvements
A short TNT fuse popup was not enough in a moving debris field. The console
game now shows a **persistent TNT FUSE xN alert**, flashing when an armed
charge has at most eight game frames remaining. The native C engine reuses
its existing moving-TNT update loop to cache the active individual charge
count and the shortest remaining fuse. The browser already had a matching
live alert. Each charge remains independent even if several occupy one
single falling chunk; the HUD goes away when all fuses finish. Pausing and
freeze frames do not run a separate hidden timer.

The Nintendo DS top-screen indicator fits the existing upper HUD; the 3DS
and Dreamcast use small overlays that appear **only while charges burn**.
There is no new render-time scan, world array, sprite atlas, framebuffer,
or continuous animation when the TNT system is quiet.

## Graphics consistency
The DS already uses a detailed, 56px-body bounded inflation rendering.
The 3DS formerly painted a smooth **full circle** over the body's detailed
sprite; a lightweight integer oval now uses the current human/Wolfman
palette and draws a lower-torso contour with highlight, fabric hem, navel
and pressure creases. It clips vertically aligned BGR8 framebuffer spans
without floating-point square roots or an intermediate image.

Dreamcast formerly used a swelling **round puff up to ~78px across**.
It now reuses the same resident puff sprite as a compact ellipse up to
~54px wide with lower-torso shading, waistband and a small strain mark.
Skin colors are cached when applying the outfit so Wolfman's grey fur is
not painted over with human pink skin.

## Regression gates
- `test/mobile_tnt_test.c` tests two independent active charges in the
  *same* chunk, urgency, and clearing the alert at the exact expiry tick,
  in both float and DS fixed-point.
- `test/n3ds_render.c` pressure probe compares normal vs inflated
  production character pixels, guards changes to a conservative torso-only
  rectangle, verifies highlight/shadow/fur and tests edge clipping.
- `test/dc_render.c` pressure probe uses the real PVR host rasterizer
  to assert compact geometry and a distinct Wolfman gray-fur palette.
- The existing DS inflation probe, multiport build, ROM bundle and
  host/asset checks continue to apply.

**Unverified:** sustained frame rate, user playthrough, and image quality
on real physical consoles. No FPS claim is made from CI alone.

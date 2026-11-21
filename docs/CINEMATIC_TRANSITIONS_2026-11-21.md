# Shared cinematic transitions — source implementation (2026-11-21)

The previous `trans()` recognized fade, wipe, iris, white flash and hard cut labels, but **rendered the same opaque/dithered whole-screen flash** for fades, wipes, irises and white flashes (white was the only color difference). This made all story scenes feel like the same placeholder transition.

## Implemented
- **Wipe**: scene is revealed from left to right over 12 simulation ticks with smoothstep easing, using one masked rectangle directly in the existing 256×192 palette framebuffer.
- **Iris**: scene opens from the center via a 4-pixel-row, integer-only elliptical aperture. The image is completely unmasked at the end (including corners), with no sqrt or per-pixel alpha blend.
- **Fade / white impact flash**: keep separate black/white exposure and short dithered release, now lasting fewer frames so dialogue and action are visible sooner.
- **Hard cut**: zero transition mask; shot remains unobscured.
- Applies to the opening story, Wolfman, and chapter 1/2 ending scenes via the same `ds/source/intro.c` implementation compiled by Nintendo DS, Nintendo 3DS, and Dreamcast.
- Adds no sprite sheets, heap allocation, framebuffer or save data. Wipe draws one rectangle; iris draws at most 48 row bands per frame for 12 frames.

## Regression gates
- `test/cutscene_transition_test.c` directly tests the shipped `trans()` renderer: wipe geometry, iris center/corners, monotonic reveal at every 60 Hz tick, full uncover at 12 ticks, distinct black/white/cut semantics.
- `test/cutscene_guard_test.c` already checks story modes, all scene progressions and guarded framebuffers. The normal host suite runs both.
- The DS ROM, 3DS runtime and Dreamcast runtime must compile and be visually reviewed. Pixel output intentionally changes for the first 12 ticks of each scene.
- Compare host `cutscene_profile` draw times, including the early transition window, and on-console rendered FPS: no hardware performance gain is claimed from this source-only change.
- Snapshot cutscenes at ticks 0, 3, 6, 9, 11, 12 for wipe and iris; review the opening suburb, breakup, transformation and closing shots.

## Out of scope
This is a concrete presentation improvement, not a new character rig or a wholesale cinematic story rewrite. The standalone full sprite atlas review and rebuilt body artwork remain separate acceptance tasks.

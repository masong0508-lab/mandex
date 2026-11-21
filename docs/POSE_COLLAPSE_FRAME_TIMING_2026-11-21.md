# Mandex: pose, collapse feedback and console frame-time pass (21 November 2026)

This pass improves readability and visual continuity **without allocating another sprite atlas, framebuffer, particle pool, or save field**.

## Implementation

- The core keeps the existing `MxMan.gait` distance-based 4-phase sprite cycle, but adds a triangular **continuous grounded foot-lift** across the two contact steps. DS, 3DS and Dreamcast use the same helper and their existing walking/side-view artwork.
- Flight facing holds its previous direction when horizontal speed is between -1.5 and +1.5 cells/second, so rebound or apex jitter cannot flip the face every frame. A meaningful direction change still turns the character.
- Aftershock particles and warnings scale from the *actual number of cracked cells*: tiers at 8/24/48 cells, with bounded dust, two shock rings on DS for large collapses, capped glints on 3DS, and a short Dreamcast building-down cue. Physical damage, levels and collision are unchanged.
- Dreamcast and 3DS reject fully offscreen particle sprites using deliberately padded screenspace bounds. Particle simulation and allocation are unchanged: invisible effects still move, age and expire normally. The Dreamcast transparent-poly list and 3DS stereo pixel loops simply get less invisible draw work.
- The on-device Dreamcast L+R diagnostic now shows CPU time spent submitting the PVR scene (**DRAW ms**) and the previous 60-frame maximum. The 3DS L+R diagnostic shows CPU drawing time for both eyes and the lower screen, excluding VBlank wait. Those are real console measurements when run, not inferred FPS estimates.

## Quality and performance gates

- `test/visual_feedback_test.c` covers lift/stance continuity, left/right hysteresis and collapse severity, in both fixed and floating-point configurations.
- Dreamcast host software PVR: render the real particle draw routine with the cull switched on/off; require identical 640x480 RGB bytes and count rasterized sprites in both modes.
- 3DS host libctru stub: render top left/right and lower screen with cull switched on/off; require identical framebuffer hashes and count rasterized particles. Any early rejection must leave the image unchanged.
- Full host gameplay robustness, DS ROM build, shared C core and sound tests remain the release gates.

## Measurement guidance

Run the same level, outfit, camera and seed in both builds. Record normal/peak DRAW ms, FPS and simulation steps/s during the same repeated 60-frame collapse window. Draw ms is CPU work only; it excludes VBlank and does not prove GPU/PVR or emulator FPS without separate measurement. A decrease in host rasterized sprite counts is **not** a hardware FPS gain, only a verified reduction in wasted work.

If host parity fails, leave the candidate changes out of main until corrected. Never grow particle/rigid-body limits based solely on reclaimed RAM allocations from earlier patches.

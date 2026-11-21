# Character impact and collapse presentation: implementation pass

## Changes made in production source

- Existing 12-tick side → rear → opposite-side walking reversals now use **12 intermediate atlas samples rather than six duplicated pairs**, passing through the existing back-facing sprite. The turning duration and gameplay response remain the same; the same shared helper serves every console renderer.
- Shared `mx_walk_punch_lunge()` in `core/mx.h` produces a bounded, triangular 0 → 0.30 → 0 cell lunge using the already-existing 0.2-second punch timer. The movement is applied only when drawing the character, not to his physics or the interaction hitbox. It disappears during rear-facing turn interpolation or non-walking states.
- DS `spr_man_walk`, Nintendo 3DS `blit_frame` (both stereo eyes), and Dreamcast `draw_frame` consume that same helper. The character's grounded foot reference stays on the world contact plane. This branch also integrates PR #23's anchored 3DS and Dreamcast contact shadows, rather than leaving conflicting versions of the renderer on separate branches.
- Dreamcast collapse debris is spread across a small section of the damaged facade (up to two rows) instead of spawning every piece at the same point. Existing particle cap, count (8–14), lifespan and velocities stay unchanged. DS/3DS already distribute dust over a local area and are left as-is.
- Float and fixed-core tests cover punch start, peak, recovery, turn guard, airborne guard, and the 12-tick turn animation. The initial visible turn frame uses the closest 15-degree atlas view (not a 30-degree snap).
- The real Dreamcast host software-PVR menu-flow test now checks grounded vs airborne walking pixels, punch animation pixels, and all four collapse-severity tiers' bounded two-dimensional dust distribution.
- The Nintendo 3DS host sprite-volume suite now renders both projected top-screen eyes through `draw_man()` and checks grounded/airborne and punching frame hashes.

## Review and performance criteria

- This is an incremental runtime-animation and collapse readability change, **not** a full rebuilt 142-frame asset atlas, a new 3D mesh rig, or completed cinematic cutscene overhaul.
- Screenshot-compare punches at 0, 3, 6, 9 and 12 simulation ticks and both facing directions; ensure the 3DS stereo views exhibit the same lunge while retaining appropriate parallax.
- Dreamcast visual checks: large and small `EV_CRUMBLE` at camera boundaries and overlapping nearby buildings; ensure debris still matches the impact footprint and does not leak beyond the logical world viewport.
- Run exact-head host tests in float/fixed, DS build, Dreamcast/3DS compile where toolchains are available, then emulator/hardware checks. Check green results for the **latest** commit, not cancelled superseded runs. Do not claim FPS improvements from this patch: a few arithmetic instructions per walking draw and unchanged bounded particle work.
- No new framebuffer, dynamic array, texture atlas, heap allocation, save field, or changed collision simulation.

## Follow-on production work

A genuinely broader model revision needs a new atlas bake from `tools/gen_manspr.py`, inspection of the real compiled sprite turnaround sheet and screenshot comparisons across the body/head pivots before committing replacement asset bytes. The current generator already has canonical profile and hair rules; do not claim new art is shipped without regenerating `art/manspr.h`.

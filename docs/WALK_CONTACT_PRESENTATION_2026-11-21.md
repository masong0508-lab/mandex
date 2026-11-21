# Walking contact and stereo grounding — 21 November 2026

## Implementation
- **3DS**: grounded walking sprites now cast a small, fixed-color contact shadow at the world-space foot plane, independently projected by each stereo eye's `View`. The shadow does not jump with animated foot lift. Uses one small filled rectangle, no extra bitmap or frame allocation.
- **Dreamcast**: the same contact plane gets one compact translucent PVR quad, under the existing walking sprite and turning atlas frames. Character animation, collisions, and draw order are unchanged.
- **DS**: already draws an anchored contact ellipse at this position; left unchanged.
- The existing 12-tick side/back/side turn and continuous gait lift are preserved. No new texture atlas, framebuffer, static sprite array, heap allocation, or gameplay state is introduced.

## Validation gates (not yet confirmed)
1. Exact-head host tests and console build jobs must complete.
2. Screenshot comparison: walking left/right, all six turning samples, grounded vs airborne, low/high camera zoom. On 3DS compare both stereo eyes with the slider at zero and full depth.
3. Dreamcast verify transparent quad blending and depth behavior in Flycast/redream, ideally also real PVR hardware.
4. Record per-frame CPU draw time and FPS on real/emulated hardware. This patch deliberately adds tiny draw work in walking mode; **do not claim an FPS improvement**.
5. Confirm no shadows when `m->gr` is false, and no detached hovering shadow during step lift.

This is an incremental graphics milestone, **not** a full new character asset/model bake. Hair and face turnaround parity remains a separate art-validation gate.

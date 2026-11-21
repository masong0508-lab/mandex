# Human turnaround crown-light art pass — 21 November 2026

## What actually changed

- Polished **94 existing palette pixels** in the **27 shipped human turn, reverse-turn, flight and dive sprites** inside `art/manspr.h`. No render-time procedural highlights are required.
- Added **1,085 palette shading adjustments across 70 Wolfman bodies** below the skull pivot, restoring front-facing highlights and rear-edge shadows without painting over the canonical eyes, muzzle or ears.
- Closed **48 tiny enclosed alpha pinholes across 39 sprites** around the upper body and hands. The conservative four-connected filter deliberately leaves larger transparent arm/elbow gaps and the spaces between legs untouched.
- A restrained highlight now follows a consistent position across the changing crown width as the human character turns. Uses the existing palette entry `HAIRL` (index 23), replacing a few `HAIR` entries (index 11) without changing any opacity or the source image dimensions. Wolfman frames are untouched.
- `tools/turn_hair_finish.py` and `tools/sprite_pinhole_cleanup.py` provide idempotent offline finishes. `tools/gen_manspr.py` invokes both during future art bakes. Its existing cel shading now includes Wolfman torsos, without touching the canonical face region.
- `test/run_tests.sh` validates the **actual checked-in atlas** with both finishers' `--check` modes and `test/sprite_volume_continuity_test.py`, alongside the existing canonical turnaround geometry tests.

## Invariants

- Same 142 atlas frames (71 human + 71 Wolfman), same palette (39 entries), and same sprite widths/heights and anchor pivots. Only 48 specifically identified internal transparent holes are intentionally made opaque; all other alpha pixels and all larger cutouts are preserved.
- The first crown-light pass was palette-only and replaced 94 indices 11→23, preserving transparency. The subsequent sprite-quality pass also seals 48 tiny *enclosed* transparent pinholes and pre-shades 1,085 Wolfman torso/rear pixels using existing palette indices. These intentional transparency fixes do not enlarge frames or alter the game-world hitboxes. The textual C header may grow slightly where an index changes from one to two decimal digits; compiled 8-bit pixel storage is still unchanged.
- No changes to physics, runtime RAM budgets, framebuffer allocations, sprite dimensions/anchors, frame index order or per-frame rendering work.
- Existing artistic identity, eyebrows, moustache, nose and chin landmarks stay in place. The Wolfman's muzzle/ears remain unpainted by torso-volume shading.

## Review

Use `python3 tools/render_turnaround_sheet.py out/art-review/turnaround.ppm` to inspect the actual baked frames, converted with `tools/ppm_to_png.py`. Review both turn directions, front→side→rear and flying/diving. The change is subtle and limited to crown lighting; it does **not** replace the body model, reconstruct the whole 3D rig or revise cutscene staging.

**Port-specific truth:** Nintendo DS and Nintendo 3DS render the indexed `art/manspr.h` frames and receive these 94 pixel changes immediately when this PR is built. Dreamcast includes `manspr.h` for metadata/frame identifiers but renders its separate higher-resolution `art/dcart.h` atlas. That texture has **not** been modified in this patch. The canonical character rebake workflow already regenerates Dreamcast art and publishes its own preview; a matching Dreamcast lighting change needs separately inspected HD-source work, not an assertion of automatic parity. Visual hardware checks and the full host/console build suite remain required before merging.

## Dreamcast high-resolution PVR follow-through

- The separate `tools/gen_dc_art.py` bake now calls `tools/hd_wolf_finish.py` on Wolfman textures, after the browser has painted each true-color sprite and before it is packed into the existing 1024×512 PVR atlas.
- Applies a restrained key/fill light to the visible torso contours, **starting below the sprite's body anchor** so eyes, muzzle, ears and teeth retain their canonical source look.
- The alpha channel, rectangle dimensions, atlas UVs and packing order remain unchanged. This changes only RGB values in the **next generated** `art/dcart.h`; the presently committed Dreamcast HD texture hasn't been rebaked in this PR.
- `test/hd_wolf_volume_test.py` exercises the real offline function with synthetic RGBA rows, checking head pixels, transparent/translucent edges, contour lighting and zero alpha movement. The rebake workflow will run that test and regenerate/review the real HD texture before publishing.

## Runtime-accurate review coloring

The original offline turnaround preview drew Wolfman sprites using the default tan `MS_PAL` skin shades. Actual DS and 3DS gameplay remap those colors to grey fur when the Wolfman outfit is selected. The contact sheet now reads the four `FUR` RGB values from the real `mx_outfit_skin()` implementation in `core/mx_meta.c` and substitutes them **only in preview output**. Both the indexed sprite bytes and the default human palette remain unchanged. `test/turnaround_preview_palette_test.py` checks the exact PPM pixels for both variants, preventing misleading artwork reviews.

## Sprite anchor stability

A new compiled-art regression fixture, `test/sprite_turn_anchor_test.py`, verifies the top of the skull and the floor-contact pixel across all 48 human and Wolfman turnaround poses. The turnaround contact sheet additionally draws a thin floor guide beneath only the grounded turning rows, making floating feet and inconsistent vertical scaling easier to identify. The guide is **review-only** and not an in-game sprite or gameplay effect.

# Human turnaround crown-light art pass — 21 November 2026

## What actually changed

- Polished **94 existing palette pixels** in the **27 shipped human turn, reverse-turn, flight and dive sprites** inside `art/manspr.h`. No render-time procedural highlights are required.
- A restrained highlight now follows a consistent position across the changing crown width as the human character turns. Uses the existing palette entry `HAIRL` (index 23), replacing a few `HAIR` entries (index 11) without changing any opacity or the source image dimensions. Wolfman frames are untouched.
- `tools/turn_hair_finish.py` implements the same offline algorithm with an idempotence check. `tools/gen_manspr.py` invokes it during future art bakes to preserve these edits if the whole atlas is regenerated.
- `test/run_tests.sh` validates the **actual checked-in atlas** with `python3 ../tools/turn_hair_finish.py --check`, alongside the existing canonical turnaround geometry tests.

## Invariants

- Same 142 atlas frames (71 human + 71 Wolfman), same palette (39 entries), same sprite widths/heights and anchor pivots, same transparent pixel mask.
- No change to physics, hitboxes, footprints, RAM use, framebuffer or per-frame drawing cost. The generated C source retains the exact same length in bytes (94 substitutions from palette number 11 to 23).
- Existing artistic identity, eyebrows, moustache, nose and chin landmarks stay in place.

## Review

Use `python3 tools/render_turnaround_sheet.py out/art-review/turnaround.ppm` to inspect the actual baked frames, converted with `tools/ppm_to_png.py`. Review both turn directions, front→side→rear and flying/diving. The change is subtle and limited to crown lighting; it does **not** replace the body model, reconstruct the whole 3D rig or revise cutscene staging.

DS, 3DS and Dreamcast all consume the same checked-in art. Visual hardware checks and the full host/console build suite remain required before merging.

# Demolition Trials expansion — November 26, 2026

## Content and controls

160 new demolition trials (eight districts, 20 per district) supplement the original 60 campaign stages. All trial levels are independently accessible. The map library includes Tenement Row, Glass Quarter, Rail Yards, Sand Works, Powder District, Spring Heights, Delivery Works and Night Patrol. Distinct geometries, occupied floors, support materials, structural piers, TNT, sand, rubber pads, pneumatic tubes and landlord types vary the objectives. Existing campaign IDs 0–59 and story achievement boundaries are unchanged.

- Browser: PLAY → TRIALS; choose a district and its level. Responsive portrait pages and landscape 20-level grid.
- DS, 3DS, Dreamcast: use up/down on the title to jump between trial districts; left/right selects individual levels. Down from the original campaign enters the first district.
- Trial completion keeps normal star/best-score progression. DS save size stays within the 8 KB EEPROM model. Dreamcast records expand to 1,536 bytes (three 512-byte blocks) per slot; old single-block records are recognized by their historical payload length and CRC.

## Measured asset budget

| Data | Bytes |
| --- | ---: |
| Eight actual editable district JSON files | 1,370,649 |
| Raw map cells (160 × 120 × 60) | 1,152,000 |
| Occupied voxel cells across the map library | 89,365 |
| Shared packed binary command stream | 56,427 |

The raw files are source assets and are not embedded in the ROMs. The C core has one compact immutable command table plus level metadata; it decodes only the selected world into the existing grid. The standalone browser embeds the same commands as script-safe ASCII. It never needs a network connection to load a trial. No new physics body pool or framebuffer is allocated.

The shared native collision loops and browser landlord/rubble broadphase now reject impossible axis-separated contacts before squared distances. This reduces math on far rubble and guards against fixed-point distance wrap for distant bodies. The test fixture is not a hardware FPS benchmark.

## Reproduction and evidence

Run `node tools/trial_pack.js` to regenerate editable JSON, native packed data and embedded browser data from the district recipes. The generator checks every target's five-cell clearance, floor support, entity limits, distinct grids and the material-span support flood matching the engine's material limits.

- `node test/trial_pack_test.js`: all 160 browser maps decoded exactly and generated files match the source.
- Source-extracted browser checks passed for orientation, manual camera, motion animation, original 20 Wolfman levels and the 40 early campaign tactics.
- Independent local C decoder probe: generated the identical 56,427-byte command stream (FNV32 7513a2c5) and compiled the actual `build_trial` function against a minimal world harness in float and fixed modes; all 160 layouts passed both.
- Local numeric collision fixture: 250,000 safe-range comparisons per math mode matched the original squared-distance tests.
- The new full-core `test/native_trial_pack_test.c` and updated VMU migration test are registered in the host suite; full native compilation and simulation, VMU hardware, actual browser Canvas playthrough, per-level solvability, and stock-device FPS remain unverified. No new ROM build is asserted.

The expansion is on the existing draft branch. Full repository cloning is unavailable in the local environment (GitHub DNS blocked); use the CI gates before merging.

# Browser creator: safe MX1 sharing, international names and deterministic bounds
## Implemented (2026-11-21)
- The existing offline level editor still stores exactly 120×60 cells, uses eight material IDs and retains the `MX1.` share-code prefix. This does not change physics or the console save protocol.
- A new shared-data validator rejects truncated/oversized RLE grids, invalid materials and runs, solids in the launcher zone, duplicate/out-of-world or wall-embedded landlords, levels with fewer than eight solid cells, and invalid man counts. All imported levels must have between one and twelve landlords and fully cover the expected grid. Valid legacy level codes remain supported.
- Import is atomic: data is checked **before** altering `PD.custom` or saving progress. A rejected code gives a meaningful user-facing reason and does not overwrite or add a level.
- The exported payload is UTF-8, so Unicode and emoji names can be shared; decoder also reads original ASCII and Latin-1 MX1 codes. Encoding and decoding use bounded input lengths (100,000 characters) to avoid pathological parsing on low-memory mobile devices.
- No new sprites, extra framebuffers, per-frame allocations, gameplay state or new formats. These operations happen only when importing/exporting.
## Acceptance
- `node test/browser_creator_codec_test.js`: real browser source, intact Unicode, legacy MX1 compatibility, 14 malformed-data gates and no partial save mutation.
- Existing `browser_orientation_test.js`, `browser_multitouch_test.js`, `browser_walk_memory_test.js`, and all host core tests must remain passing.
- Test in actual Android/iOS browser: paste, edit, play, save, share, rotate, and navigate back. Hardware/device verification is separate from Node tests.

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

- The codec and editor reset to the canonical **120-column** custom-grid geometry after the 360-column finale; importing or sharing a level cannot silently change its dimensions based on the previous campaign chapter. The regression directly sets the active world width to 360 before decoding a normal share code.

- A dedicated **Browser creator and responsive play gates** workflow now exercises the real level-sharing codec, portrait editor, multitouch, camera controls, browser power labels and turning pose before merge; the complete host suite remains a separate release gate.

## Editor preflight (next milestone)
The **TEST** and **SAVE** actions now run the same bounded layout validation as importing/sharing a code. A landlord embedded in a wall, an empty/undersized structure, impossible coordinates, or an invalid player count produces an actionable error rather than entering a broken game or overwriting the last saved custom level. This work runs on button activation, not every rendering frame. The browser creator regression invokes the shipped preflight with valid and invalid editor grids and asserts the actual TEST/SAVE handlers call it.

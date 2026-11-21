# Mandex level creator — starter structures and reversible building

## New player-facing features
Three starter layouts are available via **STARTER BUILDS** on landscape or **LEVEL ACTIONS → STARTERS** in portrait:

- **Three-Storey Tower:** brick supports, multi-height wooden decks, breakable glass and a TNT weakness, with three landlord targets.
- **Breakaway Bridge:** breakable central walkway over two structural piers with TNT on the lower supports and two landlords on the bridge.
- **Factory Workshop:** warehouse roof, two mezzanine floors, windows, central steel supports and TNT corners, with three landlords at different heights.

Every starter uses the existing eight-material simulation, respects the reserved launcher zone, and goes through the same 120×60 codec validation used for imported levels. New starters reset the save ID to `-1` so a starter cannot overwrite the player's previously loaded custom project by accident. Loading a starter is one undoable action.

## Undo and redo
- Both desktop and mobile creator action panels now have Undo and Redo. Keyboard: `Z` / `Y`, `Ctrl+Z` / `Ctrl+Y`, `Ctrl+Shift+Z`, including Meta shortcuts on macOS.
- Snapshots restore the material grid, landlord placements, level name, men count, and save identity, not just the cells.
- A new brush operation drops the stale redo branch. Snapshots are capped at **24 states**, smaller than the previous 30-state undo cap; no per-frame allocations, save-format changes or console RAM impact.

## Regression gates
`node test/browser_creator_starters_test.js` extracts and exercises the real production code to verify three different deterministic valid starter levels, non-overwriting save identity, undo/redo continuity, redo invalidation, and bounded memory history. The full game script is parsed. A dedicated GitHub Actions creator-feature workflow runs these checks plus the existing codec and touch tests. Release acceptance also requires hands-on screenshot checks on mobile portrait and landscape.

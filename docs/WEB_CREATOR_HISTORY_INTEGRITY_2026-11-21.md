# Creator history consistency — shot allowance controls

The browser level creator's **MEN − / MEN +** controls now record genuine editor history updates. Changing the allowed shot count from 5 to 4 can be undone and redone just like a material or landlord placement. Undo and redo restore the same level identity, name, target list and material grid as before.

Both landscape and portrait actions call a shared `eAdjustMen` helper. At the legal bounds of 1 and 12, repeated taps do nothing without allocating a 7,200-cell snapshot or evicting an older Undo entry. History remains capped at 24 snapshots; the shared simulation and DS/3DS/Dreamcast code are unchanged.

Regression: `node test/browser_creator_starters_test.js` source-extracts the actual editor functions and checks Undo, Redo, bounds, and both UI handlers. GitHub Actions creator gate executes it. Manual touchscreen acceptance remains necessary.

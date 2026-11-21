# Browser custom-level coordinate stability after the wide finale
The 360-column finale changes the global world width. Previously, `startCustom()` called `levelFromData()` before `load()` could reset that width, and `levelFromData()` decoded the saved 120-column RLE data into an array sized for the *previous* world. Cells could land on the wrong rows after a transition.

Fix: `decodeCells(a,width=COLS)` supports an explicit width without changing legacy calls. `levelFromData()` decodes custom levels at their canonical width 120, carries `cols:120` into the level descriptor so `load()` initializes the correct world, and builds cells with the local width rather than mutable global COLS. The level editor also requests width 120 explicitly.

Regression `test/browser_custom_level_width_test.js` executes real production codec and level loader functions after simulating a finale width of 360, checks correct tile coordinates, targets and world width, verifies ordinary 120-column play, and parses the complete browser script. Fast CI plus full host tests cover it.

No extra in-game framebuffer, frame-loop cost, save change or handheld renderer modification. On the wide-finale transition this avoids an unnecessarily large temporary 21,600-cell decode buffer.

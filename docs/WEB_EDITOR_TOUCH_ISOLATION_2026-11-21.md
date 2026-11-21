# Editor touch isolation and safe cancellation (browser)

## Problem
Two fingers could move or finish the same editor brush/rectangle stroke; a second pointer release could accidentally commit the other finger's rectangle. Lost pointer capture and orientation changes could also leave a live stroke. This is particularly disruptive when using portrait mode or a touchscreen.

## Implemented
- Track the `pointerId` that initiated an editor brush stroke; ignore other pointer down, move and up events until that stroke ends.
- Use pointer capture only when a real drawing stroke began, rather than on every tap or toolbar action.
- Normal owner release commits the rectangle exactly once. Pointer cancellation and lost capture discard an unfinished rectangle without committing it.
- Undo/redo restoration, changing starters, reopening the editor and orientation changes clear both the pointer owner and drag state.
- No new frame buffers, material arrays, assets or per-frame physics costs. One integer/null field in the browser-only editor state.

## Tests
`test/browser_editor_touch_isolation_test.js` extracts the **real production stroke code** and checks owner-only completion, foreign touches, rectangle cancellation and runtime event wiring. Fast GitHub CI runs this alongside existing portrait and multitouch regressions. Real-device acceptance still needed (Android two-finger drag, rotation, browser system interruption and stylus).

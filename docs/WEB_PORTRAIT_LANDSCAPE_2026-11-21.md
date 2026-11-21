# Web portrait and landscape control pass — 21 November 2026

The browser edition of **Mandex** now treats portrait as a complete first-class layout, not only a zoomed gameplay camera.

## What changes

- **All game states:** the internal canvas uses a 540px-wide portrait view or the original 960×540 landscape view. On every orientation/viewport change it recomputes the canvas CSS size from the available display space, respecting iPhone/Android safe-area insets and browser chrome. Pointer positions map through the measured displayed canvas rectangle, so taps land at the same logical point at either orientation.
- **Gameplay:** portrait retains the zoomed camera, damage, collapse, controls and drag-to-aim relative to the current camera. Landscape retains the existing unzoomed framing, including the extra-wide finale's separate camera.
- **Menus:** dedicated large-button portrait home, chapter/level picker, achievements, outfits, options and custom-level screens. Large level collections use two-column pages with touch-friendly Next/Previous buttons. Vertical swipes switch pages without inadvertently activating a level or deleting a custom level.
- **Level editor:** in portrait, the source level remains the full original 120×60 cell grid. The screen shows a horizontally pannable section (the PAN buttons shift it 150 world pixels), with brushes, materials, shapes, landlord placement and tools in a separate lower control tray. Touch-down and dragging convert canvas coordinates back into exact source-world cells, even at reduced phone scale. Tool selection and editing actions (men, undo, clear, test, save, share, exit) are available on separate pages; there is no need to rotate the phone just to build a level.
- **Story:** portrait cutscenes letterbox the entire original widescreen 960×540 staging into a dedicated film panel and give the dialogue a larger separate reading area. Tap-to-continue and Skip remain available.
- **Rotation safety:** changing orientation while touching cancels active game/slider/editor drags and releases virtual walking buttons. It does not restart the level, reset stars or change the simulation rate.
- **Landscape:** the established desktop drawing and buttons are unchanged, with actual CSS fitting on small displays.

## Practical mobile controls

On portrait gameplay, aim by dragging the character's sling on the world. The tool selector remains along the bottom. Once walking is enabled, directional buttons are on the lower left and Jump, Punch and Stomp on the lower right. For portrait editor, use the PAN buttons to reach another section, select a material on BRUSH TOOLS, drag on the building area to paint, and switch to LEVEL ACTIONS to save or test.

## Validation

`node test/browser_orientation_test.js` (run from the repository root) validates parsing of the real standalone web source, portrait and landscape dimensions at multiple phone/tablet sizes, consistent pointer hit-testing under CSS scaling, editor world-panning and cell mapping, orientation-release of held input, and visible/in-bounds button rectangles across all mobile menu pages. The normal host test runner executes it alongside the fixed-step physics and existing sprite, level and cutscene tests.

The new UI adds no sprite sheets, framebuffer, new game-world arrays, or extra graphics buffers. UI pages use the existing canvas. Device-specific manual checks are still valuable, particularly iOS Safari safe areas and Android browser address-bar/keyboard resizing.

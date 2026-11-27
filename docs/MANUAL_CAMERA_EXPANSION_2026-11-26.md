# Manual camera and browser playability pass

## Player controls

- Tap/click CAMERA to inspect the world; drag one finger or the mouse to pan. Use two fingers to pinch in or out. Tap FOLLOW to return to automatic framing.
- Mouse wheel zooms around the pointer. Right mouse drag enters camera mode directly.
- Keyboard C toggles inspection; +/- (the equal/minus keys) zoom around the screen center.
- The world simulation keeps running in inspection mode. Camera touches do not shoot, dive, activate a power-up or operate the walking controls. Exit inspection to use the touch gameplay controls.
- Retrying, changing levels or rotating the device returns to automatic framing. Blur or lost pointer capture cancels the held gesture rather than firing.

## Content and presentation

The level creator now has six starter designs: the existing three and Rooftop Tenements, TNT Transfer Depot, and Rubber Bounce Arena. All three additions use the existing custom-level format, grounded foundations and clear landlord standing spaces. Undo restores the previous project and its identity. Choose a starter, then TEST PLAY or SAVE.

Landing squash/recovery uses resolved impact speed; airborne tilt is continuous through the jump apex. Neither changes collision bounds, jump impulse, gravity or damage. Camera-visible particle draws use conservative bounds covering large smoke, sparks and expanding rings; the effect update loop is untouched.

## Verification

New regression scripts:
- `node test/browser_manual_camera_test.js`
- `node test/browser_camera_expansion_test.js`

Both are registered in the host suite and browser workflow. The production source was exercised with a JavaScript event and render harness, including failures against the old code and passing results after the fix. Existing automatic camera, multitouch, orientation, gait, jump and pad/stomp parity tests also passed in that harness. The full Node/Canvas, console build and hardware performance gates remain necessary. No measured device FPS gain is claimed; no native-port code changes in this branch.

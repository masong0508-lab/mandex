# Moving TNT, collisions and in-game explanations
November 22, 2026 — Mandex (browser, DS, 3DS, Dreamcast)

## Gameplay behavior

Previously a TNT voxel carried inside a falling rigid rubble chunk could be
removed by collisions, punches or dives without ever entering the TNT
explosion queue. TNT could also be moved after its supporting wall collapsed
without ever exploding. The new rules make the charge's fate depend on what
actually happens to it:

| Trigger | Behavior |
| --- | --- |
| Supporting structure detaches; charge falls or moves freely | No automatic ignition |
| Gentle contact under 28 cells/s | No fuse from the collision |
| Rubber/sand contact | Apparent ignition impact reduced to 30%; TNT fracture damage reduced to 22% |
| Medium contact of 28–54.999 cells/s | Deterministic quarter chance to arm after the collision, so some survive |
| Hard contact at 55+ cells/s | Guaranteed fuse when charge survives the blow |
| Direct lethal impact, fist, stomp or airborne plow | Destroy the mobile voxel and queue a real blast at its current location |
| An existing explosion reaches a loose charge | Short eight-game-tick fuse, with its position checked per-voxel |
| Armed loose charge settles under forced chunk pressure | Detonates rather than silently welding away |

Normal impact fuses last approximately 24 simulation updates (about 0.4
seconds at 60 Hz). Slow motion and hit-stop can delay their wall-clock
completion; gameplay deliberately avoids independent real-time clocks.
The quarter-chance decision is reproducible for a given level, chunk slot
and impact time. A second impact can produce a different outcome. A player
can therefore knock a charge onto a soft surface and have a second chance
to exploit it without causing a guaranteed explosion.

## Hitboxes and fairness

The TNT explosion origin is the **current transformed position of the
individual cell**, not the chunk's centre of mass nor its original grid
coordinates. A collision elsewhere along the same long chunk does not ignite
remote TNT: the engine records the actual TNT voxel that made contact, and
the browser HUD counts burning charges rather than only the number of chunks. A conservative chunk bounding rejection limits work first;
only charges near the blast are considered. No explosion can ignite a
distant charge solely because a long collapsed wall has its centroid close
to the blast.

The walking punch hitbox is circle-against-rectangle, with a true curved
corner rather than a rectangle expanded equally on both axes. The web
walking blocker checks actual transformed loose-voxel coordinates, matching
the console engine's previous behavior. Fixed-point flying debris contacts
also avoid overflow on huge remote coordinates.

## UI and feedback

In the browser, a moving TNT ignition shows a world-space fuse warning and
a compact screen-space warning that remains visible while any fuses burn.
If a medium collision fails to ignite, the game briefly says “TNT SURVIVED!”
instead of pretending an explosion is imminent. DS/3DS popup at the charge,
and Dreamcast uses its existing text draw layer for a short status message.
The Options explanation states the soft/hard impact distinction.

## Port performance and storage boundaries

The per-cell fuse uses one byte of previously unused struct padding:
`sizeof(MxCell)` stays 24 bytes in host float and fixed point. The
console chunk adds a few bytes for cached TNT presence and peak impact.
Only chunks holding TNT are scanned once per gameplay update for fuses.
There are no new particle pools, level data files, framebuffers or heap
allocations, and no save-version or chapter-index changes.

The UI and detonation use existing event/particle paths. The tests cover
the behavior, **not** a measured speedup or guaranteed 30 FPS on original
hardware. A crowded rubble/chain-reaction scene on actual devices still
needs profiling.

## Tests and acceptance

- `gcc -std=gnu99 -O2 -Icore test/mobile_tnt_test.c -lm -o /tmp/tnt_f && /tmp/tnt_f`
- Repeat with `-DMX_FIXED` to exercise the Nintendo DS numeric path.
- `node test/browser_mobile_tnt_test.js` extracts and executes the real
  browser TNT and punch-hitbox production code.
- `sh test/run_tests.sh` runs the above and the full existing tests.
- `TNT impact and hitbox gates` is a fast GitHub workflow on pull requests;
  the ROMs and complete Host tests workflows must also pass before merge.

**Not yet claimed:** hardware/emulator playthrough completion, arbitrary
user-made 360-column scene parity, or measured original-console framerate.

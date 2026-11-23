# Browser collision-bucket cache — 2026-11-23

## Motivation
The browser's rubble physics created a new Map and a new array for each
occupied spatial bucket on every physics update. Dense demolition scenes can
create thousands of ephemeral allocations and garbage-collection spikes.

## Improvement
The exact existing spatial hash and insertion order now use a reusable Map
and up to 2048 reusable voxel bucket arrays. Extra buckets remain transient,
so exceptionally dense 360-column scenes cannot grow the retained bucket
cache without bound. All cached arrays are emptied after contact resolution,
and the Map is cleared to avoid retaining removed rubble cells.

This does not change hitboxes, impulse order, TNT rules, material damage,
level content, saves or the C console ports.

## Verification
test/browser_collision_hash_test.js extracts and executes the production
solver. It tests 40 deterministic repeat contacts, removal of a rubble
chunk, 2105 and 2304 occupied-cell stress scenes, a return to 120 columns,
bounded cache size and absence of stale references. Full host tests run it.
CI success is required before merge.

This is an allocation/GC optimization, **not** a measured FPS claim.
Browser worst-case frame-time profiling remains necessary.

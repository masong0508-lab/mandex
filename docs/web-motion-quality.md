# Motion quality: impacts, dives and enemies

The painted squash/stretch, rotation and signed mirror now share an affine basis with ragdoll goals, inverse projection and bone lengths. Dive continues the flight leg chains and bends both arms progressively, rather than falling back to an unrelated frozen pose. Squash is bounded under extreme pressure. Ground contact immediately plants feet; landing crouch intensity uses vertical impact speed, and recovers without changing movement or hitboxes.

The landlord renderer samples windup, attack recovery, falling posture and a bounded six-frame idle cycle; attack windups take precedence over generic panic, and thrown briefcases leave the hand. Full and cached art use the same animation sampler, with a maximum of 96 cached rasters. Secondary actor positions and briefcase rotation are interpolated between fixed ticks using weak references; teleports and new objects snap. No game rules, simulation frequency, native asset sizes or level selection changes.

Tests cover affine composition, inverse transforms and bone length, both dive directions, planting/landing, landlord animation priority, cache key bounds, and secondary actor interpolation. Previous custom-level fixture and native art pose assertion were updated to exercise the current production behavior. Native build and visual preview workflows remain required.

# Mandex 40-stage early-chapter tactical guidance

Previously, the console introduction screens displayed the same generic
instruction throughout the first two chapters, regardless of the actual
materials or level objective. Players were left to guess what the pneumatic
tubes, sand, rubber walls, high steel barriers, bridge supports, or parry
mechanics were meant to do.

This pass gives all 40 pre-Wolfman stages a **specific, concise tip**.
Examples include breaking the sand hourglass's plug, banking a shot off
rubber for a bonus, lobbing *over* tall steel instead of shooting through
it, jumping on pink pads during the on-foot stage, and punching thrown
briefcases to parry.

The console text lives in a single const ROM table in
`core/mx.c:mx_level_tactic`, and DS and Dreamcast use their original
level-entry banners. The 3DS uses a lightweight one-time introduction
panel, restricted to the existing banner duration. The browser mirrors
the hints at boot **only when the level has no more-detailed existing
tutorial**; no authored advice is overwritten. The four special
Wolfman encounter tactics and enemy milestone messages remain unchanged.

This changes no stages, scores, health, buttons, levels, art atlases,
memory pools, framebuffers, or save files. The strings are looked up
in O(1) time by level number, with no transient allocations.

`test/level_tactic_test.c` checks all 60 stages in C float and DS fixed
modes, ensuring key chapter-two mechanics and the DS banner width.
`test/browser_early_tactics_test.js` compares all 40 browser strings
against the actual C source, runs the production browser tip assignment,
and guards the existing more extensive browser tutorial text.

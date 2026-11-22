# Browser briefcase budget parity — 2026-11-22

## Defect and player impact

The C game engine on Nintendo DS, 3DS and Dreamcast has exactly `MX_MAX_CASES = 4` projectile slots. Browser landlord AI previously appended to its dynamic `cases` list without a limit. With many throwers or a crowded boss scene, browser gameplay could accumulate far more than four simultaneously flying briefcases, causing unexpected danger and additional draw/collision work not present in console builds.

A focused regression with 24 simultaneous throwers reproduced **24 active browser cases on the previous main source**, despite the console limit of four. The same scenario on this patch admits at most four, maintaining bounded projectile work.

## Fix

Browser `lordAI()` now only appends a projectile when `cases.length < 4`. The threshold is intentionally the same as `MX_MAX_CASES` in `core/mx.h`. A saturated pool does not erase any already active projectiles or play an imaginary throw sound. The existing attack windup, completion animation, cooldown, and boss follow-up timing continue. Once a case lands or is parried (removed from the existing list), that slot is available for the next real throw. The C core and all console rendering/gameplay files are unchanged.

The check is part of the existing enemy attack decision, **not** a new per-frame scan, allocation, renderer loop, or texture. Parrying, stomp boosts and the 60 campaign levels remain unchanged. This changes browser combat difficulty only where the old code exceeded the native four-projectile limit.

## Regression evidence

`test/browser_combat_recovery_test.js` adds four production-AI cases:

- At a full pool: a thrower cannot append a fifth case, existing cases keep their order and data, and no false launch sound plays.
- At three cases: one authentic ballistic throw occupies the free slot; previously flying objects remain unchanged.
- Boss at capacity: the existing second-windup schedule survives, and a slot freed before the second throw can be reused; normal boss cooldown follows.
- A crowd of 24 throwers: precisely four actual cases and four throw cues, with no extra projectile storms.

Frozen previous-main code **fails three** of the new cases and passes after the fix. All 28 combat-recovery cases pass in a source-extracted JavaScript run, as do parry, pad, jump, walk camera, character memory/pose, and orientation tests.

Merge acceptance still requires exact-head GitHub browser and Canvas checks, full host suite, console packaging and a useful gameplay smoke screenshot. These local tests are behavioral evidence, not a measured FPS gain or complete device playthrough.

The platform's launch, throws, parries and graphics continue to use their current pools; no extra save or runtime state was added.

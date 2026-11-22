# Mandex: counterpunch recovery, stomp boosts and DS startup readiness

This pass focuses on actual combat and movement behavior in the browser and
the C core shared by DS, 3DS and Dreamcast. The baseline is main `c5735b6`.
All gameplay changes reuse existing fields, timers and effects.

## Multi-hit enemies keep their final recovery window

The old target-damage function checked immunity only while current HP was
greater than one. A two-HP target could therefore lose one HP to a punch and
its last HP to the body-overlap check later in that same update. Repeated
flight contacts could bypass the final recovery window in the same way.

Ordinary contact damage now honors the existing immunity timer whenever the
target has multiple current or maximum HP. A surviving hit still applies
the original 0.7-second immunity, 0.6-second stun and shield removal. Hard
damage, such as the explicit blast/crush path, retains its bypass.

The health-capacity condition is deliberate. Ordinary one-HP enemies also
use `inv` for environmental stagger without contact immunity. Applying a
blanket guard broke the existing walking-level bot: it passed temporarily
staggered targets and could no longer complete the stage. The narrowed rule
preserves those ordinary evictions while fixing multi-hit recovery.

## Stun interrupts an attack instead of hiding it until recovery

The old AI could process an armed melee or throw windup while stunned. A
counterpunch could visibly stagger an opponent, yet its queued attack still
hit the player. A healer could likewise heal during its hit stun.

A positive stun now cancels the current windup and a boss's queued second
throw before the stun timer is decremented. If stun remains active, movement
and active attack/heal decisions pause. Existing swing feedback continues
counting down, preserving the bat-block presentation. When stun expires,
ordinary decisions resume and an attack must acquire a fresh windup.

The cancellation also applies on the tick that exhausts stun and while
fighting is disabled, preventing a stored attack from firing unexpectedly
when normal behavior resumes. No attack ranges or difficulty timers changed.

### Combat evidence

`test/combat_recovery_test.c` executes actual private core functions and the
public `mx_walk_input`/`mx_update` path. It checks real punch-plus-overlap and
counterpunch sequences, final-HP immunity, hard bypass, ordinary one-HP
behavior, melee/throw/boss interrupts, healer recovery and normal attacks.

- The immutable baseline fails 17 of 44 checks in each numeric mode.
- The patched core passes all 44 in float and fixed point.
- The existing chapter-two bot still clears all 11 landlords in the
  original 11 seconds with float and 8 seconds with fixed point.
- Combo, bank-shot, collapse-survivor and structural-shock regressions pass
  in both modes.

`test/browser_combat_recovery_test.js` extracts the actual browser damage,
walking and AI functions. Twenty cases cover the same rules, including a
real `walkStep` punch followed by overlap and a counterpunch into an armed
attack. External feedback calls are recorded by the fixture harness.

## Stomp pads deliver and preserve their stronger launch

The browser contained a stronger stomp-pad speed, and native `EV_PAD`
documented a boosted flag, but both paths lost the stomp intent before pad
detection. The slam and grounded checks cleared it first. Native pads also
always selected the ordinary speed and an unboosted event flag.

The walking step now retains this update's stomp intent in one local value.
A rubber pad launches an ordinary landing at 470 pixels/second (58.75
cells/second in C) and a stomp landing at 540 pixels/second (67.5 cells/second).
The normal cleanup of the stomp pose stays in place.

Held STOMP previously reversed a fresh bounce on the very next tick. The
existing pad cooldown now suppresses new stomp activation only while the
character is still rising. Stomp becomes available again when that cooldown
expires, or immediately if a ceiling stops the ascent or the character falls.
This uses the current 0.3-second timer and adds no input-history fields.

Browser, DS and 3DS use the existing popup to say `BOOST!` for the stronger
launch. Dreamcast consumes the existing boosted event flag for its enhanced
pad-particle burst. All effects stay within the existing pools.

### Movement evidence

- `test/walk_pad_stomp_test.c` reproduces 39 baseline assertion failures in
  each numeric mode and passes with the fix.
- `test/browser_pad_stomp_test.js` executes the actual walking/collision/
  impact functions. Eleven semantic checks pass; its frozen original solver
  demonstrates four failures.
- The browser test compares full state and feedback across 96 ordinary
  no-stomp scenes, each lasting 80 ticks, with identical results.
- Fixtures cover same-tick and existing stomps, held ascent, cooldown and
  ceiling recovery, normal airborne stomps, ordinary slams and pad cooldown.

The real Chromium input smoke adds controlled encounter and pad scenes to
its existing pointer launches and shipped-level walking sequence. Attacks,
stomps and movement use real keyboard events and production updates/drawing.
It captures recovery, counterpunch, ordinary-pad and boosted-pad frames,
checks one-HP-per-window behavior and verifies the two actual launch speeds.

## DS packaging waits for rendered startup

The [baseline main ROM workflow](https://github.com/masong0508-lab/mandex/actions/runs/44976421620)
failed its first fixed four-second screenshot even though the same source
tree passed the PR smoke. The first capture was black on both screens; the
later two showed healthy title output with 101/79 colors, 49,152 lit pixels
per screen, and 28,702 changed client pixels between frames.

`tools/run_ds_smoke.py` replaces that fixed first delay with a bounded
30-second readiness phase. It polls once per second and preserves startup
images and window diagnostics. Only a missing initial window or well-formed
blank/diagnostic startup capture can retry. Process exits, command failures,
malformed images and wrong capture dimensions remain fatal.

After both screens are ready, three **new** captures at 2-, 3- and 5-second
intervals must satisfy the same screen-content and animation thresholds.
Readiness does not count as one of those samples. A blank screen or missing
window after readiness fails; frozen captures fail. The outer 90-second
timeout and mandatory packaging dependency remain in force.

The expanded standard-library test has 16 passing cases, including delayed
readiness, permanent blank/diagnostic output, one dead screen, frozen title,
corrupt data, missing windows, process exit and screenshot failure. CI retains
the captures, emulator log and structured report on success or failure.

## Reproduce

```sh
sh test/run_tests.sh
node test/browser_combat_recovery_test.js
node test/browser_pad_stomp_test.js
python3 test/ds_smoke_test.py
```

Both native tests include `mx.c` directly; compile them without a second
`core/mx.c` linker input:

```sh
gcc -std=gnu99 -O2 -Wno-unused-function -Icore \
  test/combat_recovery_test.c -lm -o /tmp/combat-recovery
/tmp/combat-recovery
gcc -std=gnu99 -O2 -Wno-unused-function -DMX_FIXED -Icore \
  test/walk_pad_stomp_test.c -lm -o /tmp/walk-pad-stomp
/tmp/walk-pad-stomp
```

Run each native fixture in both numeric modes by adding/removing
`-DMX_FIXED`. Real browser input/pixel checks require Playwright and Chromium
and run in `browser-creator-gates.yml`. The DS runner requires DeSmuME,
Xvfb, xwininfo and ImageMagick and runs in the ROM workflow.

These tests establish behavior in the covered encounters and playthroughs.
DS release-ROM gameplay input automation, physical-console FPS, sound timing
and on-device save behavior remain separate acceptance work. A boot/title
check does not establish completion of release-ROM gameplay.

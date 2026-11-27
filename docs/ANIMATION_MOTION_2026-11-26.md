# Animation motion and interactive-world pass — 2026-11-26

## Browser

- Slingshot bands follow the held character and recoil to a relaxed position after launch.
- Diving starts from the flight pitch, accelerates over 0.12 seconds and preserves inflation; mirrored flight ragdoll transforms match the sprite.
- Landlord arm windup, release and recovery are evaluated from real attack timers. The extension reaches the strike at the projectile/melee release, and a boss can recover into its second windup. Cutscene arm choreography is unchanged.
- Splat growth and dripping, easing shockwave radius/opacity, live TNT fuse illumination, upward cycling bounce-pad chevrons based on remaining rubber cells, and real pneumatic tube transfer pulses.
- No new full-screen buffer, texture atlas, or particle pool. The effects are cosmetic, and moving-TNT collision and countdown are unchanged.

## Shared native core (DS, 3DS, Dreamcast)

The same initial dive pitch and 0.12s angular acceleration now run in the float/fixed shared core. Original dive impulse, final tumble rate and one-shot input semantics remain intact. One bounded cosmetic timer is added to the live actor; no save-format change.

## Verification

- The production-source JavaScript harness ran both new animation regression scripts and existing flight, turnaround, combat, TNT, camera, gait and rubble-render regressions.
- A focused compiled C probe using extracted dive/pose functions and numeric helpers reproduced the angle discontinuity in both float and fixed modes before the patch and passed afterward. The full-core test is checked in and registered in the host runner, but full repository compilation has not been performed locally.
- Full browser/Canvas screenshot checks, CI and emulator/hardware tests are outstanding. No device frame-rate gain, completed ROM or screenshot verification is claimed.

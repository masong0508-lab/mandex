/* MANDEX DS - sound effects + tiny chiptune loop on the ARM9-driven PSG/noise channels. */
#ifndef MX_SFX_H
#define MX_SFX_H

enum { SFX_CRUNCH, SFX_LAUNCH, SFX_DIVE, SFX_THUD, SFX_BOOM, SFX_SPLAT, SFX_TARGET, SFX_POP, SFX_WIN, SFX_LOSE, SFX_CLICK, SFX_STRETCH, SFX_CRACK, SFX_BOUNCE, SFX_SLAP, SFX_BOING, SFX_GRUNT, SFX_BURP, SFX_FART, SFX_FLICK, SFX_YAWN, SFX_SNORE, SFX_WHOOSH, SFX_TAUNT, SFX_COUNT };

void sfx_init(void);
void sfx_play(int id, int pan);        /* pan 0..127 (64 = centre) */
void sfx_tick(void);                   /* call once per frame */
void music_set(int on);                /* start/stop the loop */
int  music_on(void);

#endif

/* MANDEX - PCM sound-effect synth for the 3DS and Dreamcast (the DS plays the same table on its PSG/noise channels).
   Renders effect `id` (the SFX_* enum in ds/source/sfx.h) as mono signed 16-bit at MX_SND_RATE into `out` and returns the number
   of samples written (at most `max`).  Pure integer maths, no allocation, no hardware: the platform only has to play the buffer. */
#ifndef MX_SND_H
#define MX_SND_H
#define MX_SND_RATE 22050
#define MX_SND_MAX  (MX_SND_RATE * 11 / 10)   /* the longest effect (WIN, 0.9 s) plus its release */
int mx_snd_render(int id, short *out, int max);
int mx_snd_cooldown(int id);                    /* frames before the same effect may be played again (same rate-limit as the DS) */
#include "mx.h"
/* Which effects a world event makes (the same mapping as the DS front-end): fills out[] (room for 2) and returns how many. */
int mx_event_sfx(const MxEvent *e, int *out);
#endif

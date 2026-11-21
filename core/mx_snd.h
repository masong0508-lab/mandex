/* MANDEX - PCM sound-effect synth for the 3DS and Dreamcast (the DS plays the same table on its PSG/noise channels).
   Renders effect `id` (the SFX_* enum in ds/source/sfx.h) as mono signed 16-bit at MX_SND_RATE into `out` and returns the number
   of samples written (at most `max`).  Pure integer maths, no allocation, no hardware: the platform only has to play the buffer. */
#ifndef MX_SND_H
#define MX_SND_H
#define MX_SND_RATE 22050
#define MX_SND_MAX  (MX_SND_RATE * 11 / 10)   /* the longest effect (WIN, 0.9 s) plus its release */
/* Exact sample count for pre-allocation: avoid a second PCM-sized scratch
 * buffer on the 3DS. The rendering and all note envelopes are unchanged. */
int mx_snd_sample_count(int id);
int mx_snd_render(int id, short *out, int max);
int mx_snd_cooldown(int id);                    /* frames before the same effect may be played again (same rate-limit as the DS) */
#define MX_MUS_SAMPLES (8 * 8 * (12 * MX_SND_RATE / 60))   /* the theme's loop: 12.8 s */
int mx_music_render(short *out, int max);       /* the whole theme as mono s16 at MX_SND_RATE, seamless when looped; returns samples (0 if max < MX_MUS_SAMPLES) */
#include "mx.h"
/* Which effects a world event makes (the same mapping as the DS front-end): fills out[] (room for 2) and returns how many. */
int mx_event_sfx(const MxEvent *e, int *out);
#endif

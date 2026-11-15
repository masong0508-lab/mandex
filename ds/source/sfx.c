/*
 * MANDEX DS - sound.
 *
 * Every effect is a short list of notes (optionally gliding between them) played on a PSG or
 * noise channel through libnds' soundPlayPSG / soundPlayNoise.  There are only 6 PSG + 2 noise
 * channels on the hardware, so effects are rate-limited per type and silently dropped when no
 * channel is free (the libnds calls return a negative id in that case).
 * One PSG channel is reserved for the music loop.
 */
#include <nds.h>
#include "sfx.h"

#include "sfx_tab.h"

#define NV 6
static struct { int id, seg, left, freq, active; const Sfx *s; } vo[NV];
static int cool[SFX_COUNT];

#ifdef MX_NOSOUND
static const int snd_ok = 0;            /* diagnostic build: never touch the ARM7 sound path */
void sfx_init(void){ }
#else
static const int snd_ok = 1;
void sfx_init(void){ soundEnable(); }
#endif

/* The music is NOT played on a PSG square wave any more (50% duty at 8 samples a cycle is buzzy and full of harmonics, and cutting every
   note off dead clicks).  It loops this 32-sample PCM cycle on a PCM channel instead: a fundamental with a little 2nd and 3rd harmonic
   (sine + 0.30 sin 2x + 0.08 sin 3x, peak 108), so it sounds like a soft flute / marimba, and each note fades out to nothing so there is no click. */
static const signed char MUS_WAVE[32] __attribute__((aligned(4))) = {
    0,  34,  64,  88, 102, 108, 106,  99,  88,  77,  65,  55,  45,  34,  23,  12,
    0, -12, -23, -34, -45, -55, -65, -77, -88, -99,-106,-108,-102, -88, -64, -34 };
static int sfx_fcap(int f){ return f > 1250 ? 1250 + (f - 1250) / 6 : f; }     /* nothing above ~1.4 kHz: that is where the ear-piercing was */
void sfx_play(int id, int pan){
  if(!snd_ok) return;
  const Sfx *s = &SFX[id];
  if(cool[id] > 0) return;
  cool[id] = s->cool;
  for(int i = 0; i < NV; i++) if(!vo[i].active){
    int h = s->noise ? soundPlayNoise(s->f[0] * 3 / 5, s->vol * 11 / 20, (u8)pan) : soundPlayWave(MUS_WAVE, (u16)sfx_fcap(s->f[0]), s->vol * 11 / 20, (u8)pan);   /* tones: the soft wave, not a square */
    if(h < 0) return;                                 /* no free hardware channel */
    vo[i].id = h; vo[i].seg = 0; vo[i].left = s->len[0]; vo[i].freq = s->f[0]; vo[i].s = s; vo[i].active = 1;
    return;
  }
}

/* ------------------------------------------------------------------ music */
static int mus_on, mus_step, mus_t, mus_id = -1, mus_len;
#define MUS_STEP_FRAMES 12
static const u16 NF[] = { 220, 233, 247, 262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494, 523, 554, 587, 622, 659, 698, 740, 784, 831, 880 };
#define MUS_VOL 40
/* A little theme in A minor, 8 bars of 8 steps (Am F C G Am F G Am): a melody on one PCM channel and a bass on another, each note decaying like a plucked
   marimba. Melody values are semitones above A3 (-1 = hold the last note); bass is a root and a fifth per bar in Hz. */
#define MUS_BARS 8
static const signed char TUNE[MUS_BARS * 8] = {
  12, -1, 15, -1, 19, -1, 17, 15,   17, -1, 15, -1, 12, -1, -1, -1,
  15, -1, 19, -1, 22, -1, 19, 15,   17, -1, 14, -1, 10, -1, -1, -1,
  12, -1, 15, -1, 19, -1, 20, 19,   17, -1, 20, -1, 24, -1, 20, 17,
  22, -1, 19, -1, 17, -1, 14, -1,   12, -1, 15, -1, 12, -1, -1, -1 };
static const u16 BASS[MUS_BARS][2] = { {110,165}, {87,131}, {131,196}, {98,147}, {110,165}, {87,131}, {98,147}, {110,165} };
static struct { int id, t, len, vol; } mv[2] = { { -1, 0, 0, 0 }, { -1, 0, 0, 0 } };      /* 0 melody, 1 bass */
static void mus_note(int v, int hz, int steps, int vol){
  if(mv[v].id >= 0) soundKill(mv[v].id);
  mv[v].id = soundPlayWave(MUS_WAVE, (u16)hz, (u8)(vol / 2), v ? 40 : 80); mv[v].t = 0; mv[v].len = steps * MUS_STEP_FRAMES; mv[v].vol = vol;
}
static void mus_env(int v){
  if(mv[v].id < 0) return;
  int t = mv[v].t++, L = mv[v].len;
  if(t >= L){ soundKill(mv[v].id); mv[v].id = -1; return; }
  int e = 100 - 60 * t / L;                                   /* pluck: 100% -> 40% over the note */
  if(t < 2) e = e * (t + 1) / 3;                              /* two-frame attack: no click */
  if(L - t <= 3) e = e * (L - t) / 4;                         /* and a release */
  soundSetVol(mv[v].id, (u8)(mv[v].vol * e / 100));
}

void music_set(int on){
  if(!snd_ok){ mus_on = 0; return; }
  for(int v = 0; v < 2; v++) if(mv[v].id >= 0){ soundKill(mv[v].id); mv[v].id = -1; }
  mus_on = on;
  mus_t = 0; mus_step = 0;
}
int music_on(void){ return mus_on; }

void sfx_tick(void){
  for(int i = 0; i < SFX_COUNT; i++) if(cool[i] > 0) cool[i]--;
  for(int i = 0; i < NV; i++){
    if(!vo[i].active) continue;
    const Sfx *s = vo[i].s;
    if(--vo[i].left <= 0){
      if(++vo[i].seg >= s->n){ soundKill(vo[i].id); vo[i].active = 0; continue; }
      vo[i].left = s->len[vo[i].seg];
      if(!s->glide){ vo[i].freq = s->noise ? s->f[vo[i].seg] * 3 / 5 : sfx_fcap(s->f[vo[i].seg]); soundSetFreq(vo[i].id, vo[i].freq); }
    } else if(s->glide && vo[i].seg + 1 < s->n){
      int target = s->noise ? s->f[vo[i].seg + 1] * 3 / 5 : sfx_fcap(s->f[vo[i].seg + 1]), cur = vo[i].freq;      /* glide toward the next note over the segment */
      vo[i].freq = cur + (target - cur) / (vo[i].left + 1);
      soundSetFreq(vo[i].id, vo[i].freq);
    }
  }
  if(mus_on){
    if(mus_t == 0){
      int n = TUNE[mus_step];
      if(n >= 0){ int k = 1; while(k < 8 && TUNE[(mus_step + k) % (MUS_BARS * 8)] < 0) k++; mus_note(0, NF[n], k, MUS_VOL); }     /* a note lasts until the next one */
      if((mus_step & 3) == 0) mus_note(1, BASS[mus_step >> 3][(mus_step >> 2) & 1], 4, MUS_VOL * 3 / 4);
    }
    mus_env(0); mus_env(1);
    if(++mus_t >= MUS_STEP_FRAMES){ mus_t = 0; mus_step = (mus_step + 1) % (MUS_BARS * 8); }
  }
}

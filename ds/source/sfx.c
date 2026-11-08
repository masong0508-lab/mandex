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

typedef struct { u8 noise, vol, n, glide; u16 f[4]; u8 len[4]; u8 cool; } Sfx;
static const Sfx SFX[SFX_COUNT] = {
  /* CRUNCH  */ { 1, 60,  2, 1, { 3200, 1800 },          { 2, 1 },          2 },
  /* LAUNCH  */ { 0, 90,  2, 1, { 300, 1100 },           { 9, 1 },          0 },
  /* DIVE    */ { 0, 90,  2, 1, { 1000, 180 },           { 14, 1 },         0 },
  /* THUD    */ { 1, 70,  2, 1, { 700, 200 },            { 5, 1 },          4 },
  /* BOOM    */ { 1, 127, 3, 1, { 1500, 500, 80 },       { 6, 18, 1 },      6 },
  /* SPLAT   */ { 1, 90,  2, 1, { 900, 300 },            { 7, 1 },          0 },
  /* TARGET  */ { 0, 100, 4, 0, { 880, 1175, 1568, 2093 }, { 3, 3, 3, 9 },  0 },
  /* POP     */ { 0, 80,  2, 1, { 400, 120 },            { 6, 1 },          0 },
  /* WIN     */ { 0, 100, 4, 0, { 523, 659, 784, 1047 }, { 8, 8, 8, 30 },   0 },
  /* LOSE    */ { 0, 100, 4, 1, { 392, 330, 262, 165 },  { 10, 10, 10, 30 }, 0 },
  /* CLICK   */ { 0, 70,  1, 0, { 1500 },                { 2 },             0 },
  /* STRETCH */ { 0, 40,  1, 0, { 500 },                 { 2 },             3 },
  /* CRACK   */ { 1, 55,  2, 1, { 2400, 700 },           { 3, 1 },          5 },
  /* BOUNCE  */ { 0, 90,  3, 1, { 180, 700, 260 },       { 5, 7, 1 },       4 },
  /* SLAP    */ { 1, 80,  2, 1, { 1100, 300 },           { 3, 1 },          3 },
  /* BOING   */ { 0, 70,  3, 1, { 180, 620, 300 },       { 3, 4, 1 },       2 },
  /* GRUNT   */ { 0, 60,  2, 1, { 160, 90 },             { 12, 1 },         0 },
  /* BURP    */ { 0, 100, 4, 1, { 140, 110, 160, 70 },   { 6, 8, 8, 1 },    0 },
  /* FART    */ { 1, 110, 4, 1, { 700, 420, 640, 160 },   { 5, 5, 8, 1 },    0 },
  /* FLICK   */ { 0, 60,  2, 1, { 1400, 2800 },          { 2, 1 },          0 },
  /* YAWN    */ { 0, 60,  3, 1, { 260, 520, 180 },       { 18, 24, 14 },    0 },
  /* SNORE   */ { 1, 50,  2, 1, { 500, 220 },            { 18, 16 },        0 },
  /* WHOOSH  */ { 1, 70,  2, 1, { 500, 3200 },           { 9, 1 },          0 },
  /* TAUNT   */ { 0, 90,  4, 0, { 262, 330, 247, 311 },  { 3, 3, 3, 6 },    0 },
};

#define NV 6
static struct { int id, seg, left, freq, active; const Sfx *s; } vo[NV];
static int cool[SFX_COUNT];

void sfx_init(void){ soundEnable(); }

void sfx_play(int id, int pan){
  const Sfx *s = &SFX[id];
  if(cool[id] > 0) return;
  cool[id] = s->cool;
  for(int i = 0; i < NV; i++) if(!vo[i].active){
    int h = s->noise ? soundPlayNoise(s->f[0], s->vol, (u8)pan) : soundPlayPSG(DutyCycle_50, s->f[0], s->vol, (u8)pan);
    if(h < 0) return;                                 /* no free hardware channel */
    vo[i].id = h; vo[i].seg = 0; vo[i].left = s->len[0]; vo[i].freq = s->f[0]; vo[i].s = s; vo[i].active = 1;
    return;
  }
}

/* ------------------------------------------------------------------ music */
static int mus_on, mus_step, mus_t, mus_id = -1, mus_len;
#define MUS_STEP_FRAMES 9
static const u16 NF[] = { 220, 233, 247, 262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494, 523, 554, 587, 622, 659, 698, 740, 784, 831, 880 };
static const signed char TUNE[32] = {
  12, -1, 15, -1, 14, 12, 10, -1,   7, -1, 10, -1, 12, -1, -1, -1,
  12, -1, 15, -1, 17, 15, 14, -1,  12, 10, 12, 14, 12, -1, -1, -1 };

void music_set(int on){
  mus_on = on;
  if(!on && mus_id >= 0){ soundKill(mus_id); mus_id = -1; }
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
      if(!s->glide){ vo[i].freq = s->f[vo[i].seg]; soundSetFreq(vo[i].id, vo[i].freq); }
    } else if(s->glide && vo[i].seg + 1 < s->n){
      int target = s->f[vo[i].seg + 1], cur = vo[i].freq;      /* glide toward the next note over the segment */
      vo[i].freq = cur + (target - cur) / (vo[i].left + 1);
      soundSetFreq(vo[i].id, vo[i].freq);
    }
  }
  if(mus_on){
    if(mus_t == 0){
      int n = TUNE[mus_step];
      if(n >= 0){
        mus_id = soundPlayPSG(DutyCycle_50, NF[n], 34, 64);
        mus_len = MUS_STEP_FRAMES - 2;
      }
    }
    if(mus_id >= 0 && mus_t == mus_len){ soundKill(mus_id); mus_id = -1; }
    if(++mus_t >= MUS_STEP_FRAMES){ mus_t = 0; mus_step = (mus_step + 1) & 31; }
  }
}

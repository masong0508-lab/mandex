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

void sfx_play(int id, int pan){
  if(!snd_ok) return;
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
/* The music is NOT played on a PSG square wave any more (50% duty at 8 samples a cycle is buzzy and full of harmonics, and cutting every
   note off dead clicks).  It loops this 32-sample PCM cycle on a PCM channel instead: a fundamental with a little 2nd and 3rd harmonic
   (sine + 0.30 sin 2x + 0.08 sin 3x, peak 108), so it sounds like a soft flute / marimba, and each note fades out to nothing so there is no click. */
static const signed char MUS_WAVE[32] __attribute__((aligned(4))) = {
    0,  34,  64,  88, 102, 108, 106,  99,  88,  77,  65,  55,  45,  34,  23,  12,
    0, -12, -23, -34, -45, -55, -65, -77, -88, -99,-106,-108,-102, -88, -64, -34 };
#define MUS_VOL 44                      /* start volume of a note (was 34 on a full-scale square wave; a sine has far less energy) */
static const signed char TUNE[32] = {
  12, -1, 15, -1, 14, 12, 10, -1,   7, -1, 10, -1, 12, -1, -1, -1,
  12, -1, 15, -1, 17, 15, 14, -1,  12, 10, 12, 14, 12, -1, -1, -1 };

void music_set(int on){
  if(!snd_ok){ mus_on = 0; return; }
  if(mus_id >= 0){ soundKill(mus_id); mus_id = -1; }     /* always stop the sounding note: restarting (retry, next level) used to drop its handle, and a looping PCM note then never ends */
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
        mus_id = soundPlayWave(MUS_WAVE, NF[n], MUS_VOL, 64);
        mus_len = MUS_STEP_FRAMES - 2;
      }
    } else if(mus_id >= 0 && mus_t < mus_len)
      soundSetVol(mus_id, (u8)(MUS_VOL * (mus_len - mus_t) / mus_len));      /* fade the note out over its length */
    if(mus_id >= 0 && mus_t == mus_len){ soundKill(mus_id); mus_id = -1; }
    if(++mus_t >= MUS_STEP_FRAMES){ mus_t = 0; mus_step = (mus_step + 1) & 31; }
  }
}

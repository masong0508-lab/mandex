#include "mx_snd.h"
#include "mx.h"
#include "../ds/source/sfx_tab.h"
#include "../ds/source/mus_tab.h"

/* Each table segment lasts len[i] sixtieths of a second and (when `glide`) slides to the next note's pitch; the last note just holds.
   Tones are a 50% square run through a one-pole low-pass (a bare square at 22 kHz is shrill), noise is a 15-bit LFSR held at the note rate.
   The whole effect fades linearly so it never ends on a click, and has a 2 ms attack for the same reason. */
int mx_snd_cooldown(int id){ return id >= 0 && id < SFX_COUNT ? SFX[id].cool : 0; }

int mx_snd_render(int id, short *out, int max){
  if(id < 0 || id >= SFX_COUNT || max <= 0) return 0;
  const Sfx *s = &SFX[id];
  int segn[4], total = 0;
  for(int i = 0; i < s->n; i++){ segn[i] = s->len[i] * MX_SND_RATE / 60; if(segn[i] < 1) segn[i] = 1; total += segn[i]; }
  total += MX_SND_RATE / 40;                                  /* a short release after the last segment */
  if(total > max) total = max;
  int amp = s->vol * 150;                                     /* vol 127 -> 19050: well under full scale, so layered effects never clip into a buzz */
  unsigned lfsr = 0x4001; int lp = 0, lp2 = 0, noise_v = 0; long phase = 0; int pos = 0;
  for(int i = 0; i < s->n && pos < total; i++){
    int f0 = s->f[i], f1 = (s->glide && i + 1 < s->n) ? s->f[i + 1] : f0;
    for(int k = 0; k < segn[i] && pos < total; k++, pos++){
      int f = f0 + (int)((long)(f1 - f0) * k / segn[i]);
      int v;                                                       /* v is -256..256 */
      if(s->noise){
        phase += f;
        while(phase >= MX_SND_RATE){ phase -= MX_SND_RATE; unsigned b = (lfsr ^ (lfsr >> 1)) & 1; lfsr = (lfsr >> 1) | (b << 14); noise_v = (lfsr & 1) ? 256 : -256; }
        v = noise_v;
      } else { phase += f; if(phase >= MX_SND_RATE) phase -= MX_SND_RATE;   /* a soft tone: mostly triangle, a quarter square for body (a bare square is the shrill part) */
        int tri = (int)((phase * 2 < MX_SND_RATE ? phase * 4 - MX_SND_RATE : 3L * MX_SND_RATE - phase * 4) * 256 / MX_SND_RATE), sq = phase * 2 < MX_SND_RATE ? 256 : -256;
        v = (tri * 3 + sq) / 4; }
      int env = (int)(256L * (total - pos) / total);              /* 256 -> 0 */
      if(s->noise) env = env * env / 256;                         /* noise bursts die away fast, like a real thump or crunch, instead of hissing out */
      int att = pos < 110 ? pos * 256 / 110 : 256;                /* 5 ms attack: no click */
      int x = (int)((long)v * amp / 256 * env / 256 * att / 256);
      lp += (x - lp) * (s->noise ? 2 : 3) / 8; lp2 += (lp - lp2) * 5 / 8; out[pos] = (short)lp2;   /* two poles: the top end that stung is gone */
    }
  }
  for(; pos < total; pos++){ lp = lp * 3 / 4; lp2 += (lp - lp2) * 5 / 8; out[pos] = (short)lp2; }   /* release tail */
  return total;
}

int mx_event_sfx(const MxEvent *e, int *o){
  static unsigned cell;
  switch(e->type){
  case EV_CELL:    if(++cell % 3 == 0){ o[0] = SFX_CRACK; return 1; } return 0;     /* a flying chunk: only now and then, or the rubble is a roar */
  case EV_CRUNCH:  o[0] = SFX_CRUNCH; return 1;
  case EV_CRACK:   o[0] = SFX_CRACK; return 1;
  case EV_BOUNCE:  o[0] = SFX_BOUNCE; return 1;
  case EV_TUBE: case EV_THROW: case EV_SWING: o[0] = SFX_WHOOSH; return 1;
  case EV_WALK: case EV_SPLAT: case EV_SPLOOSH: o[0] = SFX_SPLAT; return 1;
  case EV_PUNCH:   o[0] = e->a > 0 ? SFX_SLAP : SFX_WHOOSH; return 1;
  case EV_SLAM:    o[0] = SFX_THUD; o[1] = SFX_CRUNCH; return 2;
  case EV_PAD:     o[0] = SFX_BOING; return 1;
  case EV_HURT:    o[0] = SFX_GRUNT; o[1] = SFX_THUD; return 2;
  case EV_BONK:    o[0] = SFX_SLAP; o[1] = SFX_CRUNCH; return 2;
  case EV_OOF:     o[0] = SFX_CRUNCH; return 1;
  case EV_HEAL: case EV_TARGET: o[0] = SFX_TARGET; return 1;
  case EV_THUD:    if(e->a > R(0.3)){ o[0] = SFX_THUD; return 1; } return 0;  /* a=strength, b=0: previously all rubble thuds were silent */
  case EV_BOOM: case EV_AIRPOP: o[0] = SFX_BOOM; return 1;
  case EV_LAUNCH:  o[0] = SFX_LAUNCH; return 1;
  case EV_DIVE:    o[0] = SFX_DIVE; return 1;
  case EV_POP:     o[0] = SFX_POP; return 1;
  case EV_FART:    o[0] = SFX_FART; return 1;
  case EV_CRUMBLE: o[0] = SFX_THUD; o[1] = SFX_CRACK; return 2;
  case EV_WIN: case EV_DISTRICT: o[0] = SFX_WIN; return 1;
  case EV_LOSE:    o[0] = SFX_LOSE; return 1;
  default: return 0;
  }
}

/* ---- the theme as one seamless loop (3DS / Dreamcast): the same notes and plucked decay the DS plays on its two PCM channels.
   Melody and bass loop a 32-point soft wave (a sine with a little 2nd/3rd harmonic); each note decays 100% -> 40%, with a short attack and release. */
static const signed char MWAVE[32] = {
    0,  34,  64,  88, 102, 108, 106,  99,  88,  77,  65,  55,  45,  34,  23,  12,
    0, -12, -23, -34, -45, -55, -65, -77, -88, -99,-106,-108,-102, -88, -64, -34 };
static void mus_voice(short *out, int n, int start, int len, int hz, int vol){        /* start/len in samples */
  long ph = 0;
  for(int k = 0; k < len && start + k < n; k++){
    ph += (long)hz * 32 * 256; int idx = (int)((ph / MX_SND_RATE) & 0x1FFF);           /* 32 points, 8 fractional bits */
    int i0 = (idx >> 8) & 31, i1 = (i0 + 1) & 31, fr = idx & 255, w = MWAVE[i0] + (MWAVE[i1] - MWAVE[i0]) * fr / 256;
    int e = 100 - 60 * k / len;
    int att = MX_SND_RATE / 30, rel = MX_SND_RATE / 20;
    if(k < att) e = e * (k + 1) / (att + 1);
    if(len - k < rel) e = e * (len - k) / (rel + 1);
    out[start + k] = (short)(out[start + k] + w * 256 / 127 * vol * e / 100);
  }
}
int mx_music_render(short *out, int max){
  const int step = MUS_STEP_FRAMES * MX_SND_RATE / 60, total = MUS_BARS * 8 * step;
  if(max < total) return 0;
  for(int i = 0; i < total; i++) out[i] = 0;
  for(int t = 0; t < MUS_BARS * 8; t++){
    if(TUNE[t] >= 0){ int k = 1; while(t + k < MUS_BARS * 8 && TUNE[t + k] < 0) k++; mus_voice(out, total, t * step, k * step, NF[(int)TUNE[t]], 38); }
    if((t & 3) == 0) mus_voice(out, total, t * step, 4 * step, BASS[t >> 3][(t >> 2) & 1], 28);
  }
  return total;
}

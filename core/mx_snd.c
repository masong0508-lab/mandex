#include "mx_snd.h"
#include "mx.h"
#include "../ds/source/sfx_tab.h"

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
  case EV_THUD:    if(e->b > R(0.3)){ o[0] = SFX_THUD; return 1; } return 0;
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

/*
 * MANDEX - the man's idle gags, shared by the DS and Dreamcast front-ends (header only, no platform calls).
 *
 * A gag is a short list of steps {frame, ticks, fx}.  While he waits in the sling the player starts a random gag after
 * a few seconds of no input, never the same one twice in a row; after ~20 s of nothing he falls asleep.  The front-end
 * calls mi_update() once per 60 Hz tick, draws mi_frame(), and reacts to mi_fx() (sound / particles) - it does not
 * need to know what any gag is.
 * Include after manspr.h.
 */
#ifndef MX_MANIDLE_H
#define MX_MANIDLE_H

enum { FX_NONE, FX_SLAP, FX_BOING, FX_GRUNT, FX_BURP, FX_FART, FX_FLICK, FX_YAWN, FX_SNORE, FX_SNIFF, FX_WHOOSH, FX_TAUNT, FX_COUNT };
enum { GAG_BELLY, GAG_WIGGLE, GAG_SCRATCH, GAG_BURP, GAG_FART, GAG_PICK, GAG_YAWN, GAG_CAPE, GAG_COUNT };

typedef struct { unsigned char frame, ticks, fx; } MiStep;

#define MI_STEPS(...) { __VA_ARGS__, { 0, 0, 0 } }          /* ticks == 0 ends a gag */
#define MI_MAXSTEPS 30
static const MiStep MI_GAGS[GAG_COUNT][MI_MAXSTEPS] = {
  /* belly bounce: slap, jiggle, slap... */
  MI_STEPS({ MSF_IDLE_A, 10, 0 }, { MSF_BELLY_A, 6, FX_SLAP }, { MSF_BELLY_B, 6, 0 }, { MSF_BELLY_A, 6, FX_SLAP }, { MSF_BELLY_B, 6, 0 },
           { MSF_BELLY_A, 6, FX_SLAP }, { MSF_BELLY_B, 8, 0 }, { MSF_IDLE_A, 10, 0 }),
  /* butt wiggle: back to us, hips left-right, boing per swing */
  MI_STEPS({ MSF_BACK_C, 8, 0 }, { MSF_BACK_L, 6, FX_BOING }, { MSF_BACK_R, 6, FX_BOING }, { MSF_BACK_L, 6, FX_BOING }, { MSF_BACK_R, 6, FX_BOING },
           { MSF_BACK_L, 6, FX_BOING }, { MSF_BACK_R, 6, FX_BOING }, { MSF_BACK_C, 10, 0 }, { MSF_IDLE_A, 8, 0 }),
  /* belly scratch, then a sniff of the fingers */
  MI_STEPS({ MSF_SCRATCH_A, 8, 0 }, { MSF_SCRATCH_B, 8, 0 }, { MSF_SCRATCH_A, 8, 0 }, { MSF_SCRATCH_B, 8, 0 }, { MSF_SCRATCH_A, 8, 0 },
           { MSF_SCRATCH_B, 8, 0 }, { MSF_IDLE_A, 20, FX_SNIFF }, { MSF_IDLE_A, 10, 0 }),
  /* burp: puff up, let it out (green cloud) */
  MI_STEPS({ MSF_IDLE_A, 12, 0 }, { MSF_BURP_A, 22, FX_GRUNT }, { MSF_BURP_B, 28, FX_BURP }, { MSF_IDLE_A, 14, 0 }),
  /* fart: turn around, strain, release (big green cloud + flies), guilty glance */
  MI_STEPS({ MSF_BACK_C, 10, 0 }, { MSF_FART_A, 24, FX_GRUNT }, { MSF_FART_B, 34, FX_FART }, { MSF_BACK_C, 20, 0 }, { MSF_IDLE_A, 10, 0 }),
  /* nose pick, flick it away */
  MI_STEPS({ MSF_PICK_A, 10, 0 }, { MSF_PICK_B, 10, 0 }, { MSF_PICK_A, 10, 0 }, { MSF_PICK_B, 10, 0 }, { MSF_PICK_A, 12, 0 },
           { MSF_IDLE_A, 16, FX_FLICK }),
  /* big yawn and stretch */
  MI_STEPS({ MSF_IDLE_A, 12, 0 }, { MSF_YAWN, 64, FX_YAWN }, { MSF_IDLE_A, 12, 0 }),
  /* cape taunt: grab the cape, spin round (a smooth 3D-style turn) with it whipped off, wag the butt at you, spin back and put it on again */
  MI_STEPS({ MSF_CAPE_A, 21, FX_GRUNT }, { MSF_SPIN_0, 5, FX_WHOOSH }, { MSF_SPIN_1, 4, 0 }, { MSF_SPIN_2, 4, 0 }, { MSF_SPIN_3, 4, 0 },
           { MSF_SPIN_4, 4, 0 }, { MSF_SPIN_5, 4, 0 }, { MSF_SPIN_6, 4, 0 },
           { MSF_SPIN_BL, 9, FX_TAUNT }, { MSF_SPIN_BR, 9, FX_BOING }, { MSF_SPIN_BL, 9, FX_BOING }, { MSF_SPIN_BR, 9, FX_BOING },
           { MSF_SPIN_BL, 9, FX_BOING }, { MSF_SPIN_BR, 9, FX_BOING }, { MSF_SPIN_BL, 9, FX_BOING }, { MSF_SPIN_BR, 9, FX_BOING },
           { MSF_SPIN_6, 4, FX_WHOOSH }, { MSF_SPIN_5, 4, 0 }, { MSF_SPIN_4, 4, 0 }, { MSF_SPIN_3, 4, 0 }, { MSF_SPIN_2, 4, 0 },
           { MSF_SPIN_1, 4, 0 }, { MSF_SPIN_0, 4, 0 }, { MSF_CAPE_A, 18, 0 }, { MSF_IDLE_A, 12, 0 }),
};
static const MiStep MI_SLEEP[3] = { { MSF_SLEEP_A, 44, FX_SNORE }, { MSF_SLEEP_B, 44, 0 }, { 0, 0, 0 } };

#define MI_START_MIN   180      /* ticks of nothing before a gag (3 s) ... */
#define MI_START_RAND  120      /* ... plus up to 2 s more */
#define MI_SLEEP_AFTER 1200     /* 20 s of nothing in total */

typedef struct {
  int gag;                      /* -1 none, 0..GAG_COUNT-1, or GAG_COUNT = sleeping */
  int step, t;                  /* current step, ticks spent in it */
  int quiet, total;             /* ticks since the last gag ended / since the last input */
  int wait, last;               /* ticks to wait before the next gag, previous gag */
  unsigned rng;
  int fx;                       /* fx fired on this tick (FX_NONE when nothing) */
  int base_a, base_b;           /* what "idle" means for this screen (IDLE_A/B, or FLEX_A/B on the title) */
} MiState;

static int mi_rand(MiState *s, int n){ s->rng = s->rng * 1664525u + 1013904223u; return (int)((s->rng >> 16) % (unsigned)n); }

static void mi_reset(MiState *s){                /* any input / new level: back to idle, timers restart */
  s->gag = -1; s->step = s->t = s->quiet = s->total = 0; s->fx = FX_NONE;
  if(s->rng == 0) s->rng = 0x2545F491u;
  s->wait = MI_START_MIN + mi_rand(s, MI_START_RAND);
  if(s->base_a == s->base_b && s->base_a == 0) { s->base_a = MSF_IDLE_A; s->base_b = MSF_IDLE_B; }
}

static const MiStep *mi_cur(const MiState *s){
  return s->gag < 0 ? 0 : s->gag == GAG_COUNT ? &MI_SLEEP[s->step] : &MI_GAGS[s->gag][s->step];
}

/* call once per 60 Hz tick while he is idle in the sling */
static void mi_update(MiState *s){
  s->fx = FX_NONE;
  s->total++;
  if(s->gag < 0){
    if(s->total > MI_SLEEP_AFTER){ s->gag = GAG_COUNT; s->step = 0; s->t = 0; s->fx = MI_SLEEP[0].fx; return; }
    if(++s->quiet >= s->wait){
      int g; do g = mi_rand(s, GAG_COUNT); while(g == s->last);
      s->gag = s->last = g; s->step = 0; s->t = 0; s->fx = MI_GAGS[g][0].fx;
    }
    return;
  }
  const MiStep *st = mi_cur(s);
  if(++s->t >= st->ticks){
    s->t = 0; s->step++;
    st = mi_cur(s);
    if(st->ticks == 0){                                  /* gag over */
      if(s->gag == GAG_COUNT){ s->step = 0; st = mi_cur(s); }       /* sleep loops until he is woken */
      else { s->gag = -1; s->quiet = 0; s->wait = MI_START_MIN + mi_rand(s, MI_START_RAND); return; }
    }
    s->fx = st->fx;
  }
}

/* the frame to draw; `ph` is a free-running tick counter (for the little belly sway while truly idle) */
static int mi_frame(const MiState *s, int ph){
  const MiStep *st = mi_cur(s);
  if(!st) return ((ph >> 4) & 1) ? s->base_a : s->base_b;
  /* gag steps say IDLE_A for "neutral": on the title screen that means the flexing pose instead */
  return st->frame == MSF_IDLE_A ? s->base_a : st->frame;
}
static int mi_active(const MiState *s){ return s->gag >= 0; }

/* victory dance (butt wiggle loop) and the sad slump for the end panels */
static int mi_dance_frame(int ph){ static const unsigned char seq[4] = { MSF_BACK_L, MSF_BACK_C, MSF_BACK_R, MSF_BACK_C }; return seq[(ph / 7) & 3]; }

#endif

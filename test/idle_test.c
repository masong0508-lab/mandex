/* Checks the idle-gag tables and the player (art/manidle.h): every step points at a real frame with a real length,
   every gag ends, no gag repeats back to back, all gags show up, the man falls asleep when left alone, input wakes him. */
#include <stdio.h>
#include <stdlib.h>
#include "manspr.h"
#include "manidle.h"
static int bad;
#define CHECK(c, ...) do{ if(!(c)){ printf("FAIL: " __VA_ARGS__); printf("\n"); bad++; } }while(0)
int main(void){
  for(int g = 0; g < GAG_COUNT; g++){
    int n = 0, total = 0;
    while(MI_GAGS[g][n].ticks){ const MiStep *s = &MI_GAGS[g][n];
      CHECK(s->frame < MSF_COUNT, "gag %d step %d: frame %d out of range", g, n, s->frame);
      CHECK(s->fx < FX_COUNT, "gag %d step %d: fx %d out of range", g, n, s->fx);
      total += s->ticks; n++; if(n >= MI_MAXSTEPS) break; }
    CHECK(n > 0 && n < MI_MAXSTEPS, "gag %d has %d steps (no terminator?)", g, n);
    printf("gag %d: %d steps, %.1f s\n", g, n, total / 60.0);
  }
  for(int n = 0; MI_SLEEP[n].ticks; n++) CHECK(MI_SLEEP[n].frame < MSF_COUNT, "sleep frame");
  for(int p = 0; p < 40; p++) CHECK(mi_dance_frame(p) < MSF_COUNT, "dance frame");
  /* run 10 minutes of nobody touching anything */
  MiState s = {0}; mi_reset(&s);
  int started[GAG_COUNT + 1] = {0}, prev = -1, lastg = -1, fxs[FX_COUNT] = {0}, slept = -1;
  for(int t = 0; t < 60 * 600; t++){
    mi_update(&s);
    if(s.fx) fxs[s.fx]++;
    int f = mi_frame(&s, t); CHECK(f >= 0 && f < MSF_COUNT, "frame %d out of range at tick %d", f, t);
    if(s.gag != prev && s.gag >= 0){ started[s.gag]++; if(s.gag < GAG_COUNT){ CHECK(s.gag != lastg, "gag %d repeated at tick %d", s.gag, t); lastg = s.gag; } if(s.gag == GAG_COUNT && slept < 0) slept = t; }
    prev = s.gag;
    if(slept < 0 && s.total == 0) CHECK(0, "total reset");
  }
  CHECK(slept > 0 && slept < 60 * 25, "never fell asleep (%d)", slept);
  /* gags before he nods off are few, so run many short sessions to see that all of them come up */
  int seen[GAG_COUNT] = {0};
  for(int seed = 1; seed < 400; seed++){
    MiState q = {0}; q.rng = seed * 7919u; mi_reset(&q);
    for(int t = 0; t < 1100; t++){ mi_update(&q); if(q.gag >= 0 && q.gag < GAG_COUNT) seen[q.gag] = 1; }
  }
  for(int g = 0; g < GAG_COUNT; g++) CHECK(seen[g], "gag %d never played", g);
  /* waking up */
  mi_reset(&s); CHECK(s.gag == -1 && s.total == 0, "reset did not wake him");
  printf("fell asleep after %.1f s; fx counts: slap %d boing %d grunt %d burp %d fart %d flick %d yawn %d snore %d sniff %d whoosh %d taunt %d\n", slept / 60.0,
         fxs[FX_SLAP], fxs[FX_BOING], fxs[FX_GRUNT], fxs[FX_BURP], fxs[FX_FART], fxs[FX_FLICK], fxs[FX_YAWN], fxs[FX_SNORE], fxs[FX_SNIFF], fxs[FX_WHOOSH], fxs[FX_TAUNT]);
  printf(bad ? "idle test FAILED (%d)\n" : "idle test passed\n", bad);
  return bad != 0;
}

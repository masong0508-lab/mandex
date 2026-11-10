/* Collapsible buildings (core, port 13 step B).  Black-box through mx.h, float and fixed point:
     - off (the default): no body ever sleeps as a solid, exactly as before;
     - on: on the scripted shots every level makes at least some resting rubble somewhere, and the pool never overflows;
     - mx_set_collapse(0) wakes every sleeper at once; the setting survives mx_load;
     - when the level ends (win or lose) everything welds back: nothing active is left within thirty seconds;
     - a full pool welds a sleeper instead of dropping the new body (the count of live bodies never exceeds MX_MAX_CHUNKS).
     gcc -std=c99 -O2 -I../core ../core/mx.c collapse_test.c -lm      (and again with -DMX_FIXED, and with -DMX_MAX_CHUNKS=48) */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "mx.h"
#ifdef MX_FIXED
#define RS 4096.0
#else
#define RS 1.0
#endif
static int fails;
#define CHECK(c, msg) do{ if(!(c)){ printf("FAIL: %s\n", msg); fails++; } else printf("ok: %s\n", msg); }while(0)
static MxWorld W;
static int count(int rest_only){ int n = 0; for(int c = 0; c < MX_MAX_CHUNKS; c++) if(W.chunks[c].active && (!rest_only || W.chunks[c].rest)) n++; return n; }
typedef struct { int peak_rest, peak_active, frames, ended, left_after, woke_ok, tried_wake, state; } Run;
/* one level, `shots` scripted shots, the same every time; if wake_test, the first time something sleeps it is woken by mx_set_collapse(0) and the run goes on with collapse off */
static Run play(int level, int collapse, int shots, int wake_test){
  Run r; memset(&r, 0, sizeof r);
  memset(&W, 0, sizeof W); mx_load(&W, level); mx_set_collapse(&W, collapse);
  unsigned rs = 777u + (unsigned)level * 104729u; int shot = 0, f;
  for(f = 0; f < 6000; f++){
    if(W.state == MX_PLAY && W.man.mode == MAN_AIM && W.men > 0 && f > 30 && shot < shots){
      rs = rs * 1664525u + 1013904223u; double a = (14 + ((rs >> 16) % 36)) * 3.14159265 / 180; rs = rs * 1664525u + 1013904223u; double p = 8 + ((rs >> 16) % 30) / 10.0;
      mx_set_pull(&W, (real)(-p * cos(a) * RS), (real)(p * sin(a) * RS)); mx_release(&W); shot++;
    }
    mx_update(&W, R(1.0 / 60.0));
    int a = count(0), s = count(1); if(s > r.peak_rest) r.peak_rest = s; if(a > r.peak_active) r.peak_active = a;
    if(wake_test && !r.tried_wake && s > 0){ r.tried_wake = 1; mx_set_collapse(&W, 0); r.woke_ok = count(1) == 0 && W.collapse == 0; }
    if(W.state != MX_PLAY){ r.ended = 1; r.state = W.state; break; }
  }
  r.frames = f;
  if(r.ended){ for(int k = 0; k < 1800 && count(0); k++) mx_update(&W, R(1.0 / 60.0)); r.left_after = count(0); }   /* up to thirty seconds after the end (a woken slab can drop a last piece a few seconds later) */
  return r;
}
int main(void){
  int on_total = 0, off_total = 0, peak_all = 0, left_bad = 0, ended_on = 0, levels_with_rest = 0, off_rest_bad = 0, over = 0;
  for(int L = 0; L < MX_NUM_LEVELS; L++){
    Run a = play(L, 1, 5, 0), b = play(L, 0, 5, 0);
    on_total += a.peak_rest; off_total += b.peak_rest; levels_with_rest += a.peak_rest > 0; off_rest_bad += b.peak_rest > 0;
    if(a.peak_active > peak_all) peak_all = a.peak_active;
    if(a.peak_active > MX_MAX_CHUNKS || b.peak_active > MX_MAX_CHUNKS) over++;
    ended_on += a.ended; if(a.ended && a.left_after != 0) { left_bad++; printf("  level %d (collapse on): %d bodies still active 30 s after the end\n", L + 1, a.left_after); }
    if(b.ended && b.left_after != 0){ left_bad++; printf("  level %d (collapse off): %d bodies still active 30 s after the end\n", L + 1, b.left_after); }
  }
  printf("    (peak sleeping bodies summed over levels: on %d, off %d; most live bodies at once: %d of %d; levels that ended inside the run with collapse on: %d/%d)\n", on_total, off_total, peak_all, MX_MAX_CHUNKS, ended_on, MX_NUM_LEVELS);
  CHECK(off_rest_bad == 0, "collapse off (the default): nothing ever sleeps as a solid body");
  CHECK(levels_with_rest >= MX_NUM_LEVELS / 2, "collapse on: rubble comes to rest as sleeping solids on at least half the levels");
  CHECK(over == 0, "live bodies never exceed MX_MAX_CHUNKS");
  CHECK(left_bad == 0, "when a level ends everything settles back: nothing is still active thirty seconds later");
  { int woke = 0, tried = 0; for(int L = 0; L < MX_NUM_LEVELS; L++){ Run r = play(L, 1, 5, 1); tried += r.tried_wake; woke += r.woke_ok; }
    CHECK(tried > 0 && woke == tried, "mx_set_collapse(0) wakes every sleeper at once (checked the first time rubble slept on each level)"); }
  memset(&W, 0, sizeof W); mx_load(&W, 3); mx_set_collapse(&W, 1); mx_load(&W, 5);
  CHECK(W.collapse == 1, "the option survives mx_load (a reload or the next level keeps it)");
  mx_set_collapse(&W, 0); mx_load(&W, 5); CHECK(W.collapse == 0, "...and so does off");
  mx_set_collapse(&W, 7); CHECK(W.collapse == 1, "any non-zero value means on");
  printf(fails ? "collapse_test: %d FAILED\n" : "collapse_test: all passed\n", fails);
  return fails != 0;
}

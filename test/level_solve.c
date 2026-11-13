/* Greedy solvability check for the shared core: per man, try a grid of sling angles / pulls / belly-flop timings from a snapshot of the world,
   keep the shot that kills the most landlords (cells destroyed breaks ties) and go on.  Prints, per level, whether a solution turned up and in
   how many men.  A "no" does not prove a level impossible (the search is greedy and small), a "yes" proves a winning shot sequence exists
   with this exact physics.  usage: level_solve [first] [last] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mx.h"
#define DT R(1.0/60.0)
static MxWorld W, S, B;

/* walk levels: head for the nearest landlord, punching, hopping now and then; give up after 6 s */
static int walk_f;
static void walk_bot(MxWorld *w){
  if(w->man.mode != MAN_WALK){ walk_f = 0; return; }
  if(++walk_f > 360){ mx_walk_quit(w); return; }
  int best = -1; double bd = 1e9, mx = RTOF(w->man.x), my = RTOF(w->man.y);
  for(int i = 0; i < w->ntargets; i++) if(w->targets[i].alive){ double d = fabs(RTOF(w->targets[i].x) - mx) + 2 * fabs(RTOF(w->targets[i].y) - my); if(d < bd){ bd = d; best = i; } }
  int ax = best >= 0 ? (RTOF(w->targets[best].x) > mx ? 1 : -1) : 0;
  mx_walk_input(w, ax, walk_f % 25 == 0, 1, 0);
}
static void shoot(MxWorld *w, double pull, int deg, int dive_at){
  double a = deg * 3.14159265 / 180.0;
  mx_set_pull(w, R(-pull * cos(a)), R(pull * sin(a))); mx_release(w);
  for(int f = 0; f < 60 * 14 && w->state == MX_PLAY; f++){
    if(dive_at && f == dive_at && w->man.mode == MAN_FLY) mx_dive(w);
    walk_bot(w); mx_update(w, DT);
    if(w->man.mode == MAN_AIM) break;
  }
}
int main(int argc, char **argv){
  int lo = argc > 1 ? atoi(argv[1]) : 0, hi = argc > 2 ? atoi(argv[2]) : MX_NUM_LEVELS - 1, solved = 0, n = 0;
  static const double pulls[] = {11.5, 10.0, 8.0, 6.0};
  static const int dives[] = {0, 18, 30, 42, 56};
  for(int l = lo; l <= hi; l++){
    int men0 = 0, used = 0, ok = 0, tries = getenv("DEEP") ? atoi(getenv("DEEP")) : 1;
    for(int tr = 0; tr < tries && !ok; tr++){
    srand(1234 + tr);
    mx_load(&W, l);
    for(int i = 0; i < 120; i++) mx_update(&W, DT);
    men0 = W.men; used = 0;
    while(W.state == MX_PLAY && W.man.mode == MAN_AIM && used < men0 + 1){
      struct { long sc; double p; int d, v; } cand[256]; int nc = 0;
      memcpy(&S, &W, sizeof S);
      int alive0 = mx_targets_alive(&S);
      for(int pi = 0; pi < 4; pi++) for(int d = -60; d <= 66; d += (tr ? 4 : 6)) for(int vi = 0; vi < 5; vi++){
        if(nc >= 256) break;
        memcpy(&B, &S, sizeof B);
        shoot(&B, pulls[pi], d, dives[vi]);
        for(int f = 0; f < 60 * 8 && B.state == MX_PLAY && B.man.mode != MAN_AIM; f++){ walk_bot(&B); mx_update(&B, DT); }
        int k = alive0 - mx_targets_alive(&B), c = mx_cells_left(&S) - mx_cells_left(&B);
        if(B.state == MX_WIN) k += 100;
        cand[nc].sc = (long)k * 100000 + c; cand[nc].p = pulls[pi]; cand[nc].d = d; cand[nc].v = dives[vi]; nc++;
      }
      /* pick the best (restart 0) or a random one of the best five */
      int pick = 0;
      for(int i = 1; i < nc; i++) if(cand[i].sc > cand[pick].sc) pick = i;
      if(tr){ int top[5], nt = 0; long used_sc[5];
        for(int r = 0; r < 5 && r < nc; r++){ int bi = -1; for(int i = 0; i < nc; i++){ int skip = 0; for(int q = 0; q < nt; q++) if(top[q] == i) skip = 1; if(!skip && (bi < 0 || cand[i].sc > cand[bi].sc)) bi = i; } top[nt++] = bi; used_sc[r] = cand[bi].sc; }
        pick = top[rand() % nt]; (void)used_sc; }
      shoot(&W, cand[pick].p, cand[pick].d, cand[pick].v); used++;
      for(int f = 0; f < 60 * 8 && W.state == MX_PLAY && W.man.mode != MAN_AIM; f++){ walk_bot(&W); mx_update(&W, DT); }
      if(W.state == MX_WIN){ ok = 1; break; }
    }
    }
    n++; solved += ok;
    printf("%2d %-22s men %2d: %s%s\n", l, mx_level_name(l), men0, ok ? "solved in " : "NOT solved by the greedy search", ok ? (char[4]){(char)('0' + used / 10 % 10 ? '0' + used / 10 % 10 : ' '), (char)('0' + used % 10), 0, 0} : "");
    fflush(stdout);
  }
  printf("%d of %d solved\n", solved, n);
  return 0;
}

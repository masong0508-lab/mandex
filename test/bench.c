/* Host benchmark of the shared core: for every level, one fixed shot, then 600 frames of mx_update; prints per-level avg and worst frame time.
 * Build float:  gcc -O2 -I../core ../core/mx.c bench.c -lm -o bench_float
 * Build fixed:  gcc -O2 -DMX_FIXED -I../core ../core/mx.c bench.c -lm -o bench_fixed   (-DMX_MAX_CHUNKS=16 is the default = DS)
 * Host milliseconds are NOT console milliseconds: use them only to compare one change against another.  Prints a checksum so a "speed-up" that changes behaviour shows. */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mx.h"
static MxWorld w;
static double now(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
int main(int argc, char **argv){
  int quiet = argc > 1; double sum = 0, worst = 0; unsigned long long chk = 0;
  for(int L = 0; L < MX_NUM_LEVELS; L++){
    memset(&w, 0, sizeof w); mx_load(&w, L);
    mx_set_pull(&w, R(-9), R(5)); mx_release(&w);
    double tot = 0, wl = 0;
    for(int f = 0; f < 600; f++){
      if(w.man.mode == MAN_FLY && f == 90) mx_dive(&w);
      if(w.man.mode == MAN_WALK) mx_walk_input(&w, 1, 0, (f & 15) == 0, 0);
      double a = now(); mx_update(&w, R(1.0 / 60.0)); double d = now() - a;
      tot += d; if(d > wl) wl = d;
    }
    chk = chk * 31 + (unsigned long long)mx_cells_left(&w) * 7 + (unsigned long long)w.score;
    sum += tot; if(wl > worst) worst = wl;
    if(!quiet) printf("L%2d %-22s avg %.4f ms  worst %.3f ms  cells %d\n", L, mx_level_name(L), tot / 600, wl, mx_cells_left(&w));
  }
  printf("TOTAL %.1f ms over %d levels x 600 frames, worst frame %.3f ms, checksum %llu\n", sum, MX_NUM_LEVELS, worst, chk);
  return 0;
}

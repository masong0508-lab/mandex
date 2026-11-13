/* Pneumatic tubes (core): Special Delivery has three; a man who flies into a mouth is posted out of its nozzle, nearly weightless for 0.55 s,
   and cannot be caught by a second tube for 0.35 s.  Built twice by run_tests.sh (float and fixed). */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "mx.h"
static int fails;
#define CHECK(c, msg) do{ if(!(c)){ printf("FAIL: %s\n", msg); fails++; } else printf("ok: %s\n", msg); }while(0)
static MxWorld W;
#define DT R(1.0/60.0)
static double D(real v){ return RTOF(v); }
int main(void){
  mx_load(&W, 20);
  CHECK(W.ntubes == 3, "Special Delivery has three tubes");
  CHECK(fabs(D(W.tubes[0].ax) - 37.5) < 0.01 && fabs(D(W.tubes[0].bx) - 68.25) < 0.01 && fabs(D(W.tubes[0].vx) - 95) < 0.01, "tube 1: mouth (37.5,42.5), nozzle x 68.25, 95 cells/s");
  CHECK(fabs(D(W.tubes[2].ay) - 19) < 0.01 && fabs(D(W.tubes[2].by) - 33) < 0.01, "tube 3: mouth y 19, nozzle y 33");
  mx_load(&W, 0);
  CHECK(W.ntubes == 0, "an ordinary level has no tubes");

  for(int ti = 0; ti < 3; ti++){
    mx_load(&W, 20);
    for(int i = 0; i < 120; i++) mx_update(&W, DT);
    const MxTube t = W.tubes[ti];
    W.man.mode = MAN_FLY; W.man.x = t.ax - R(5); W.man.y = t.ay; W.man.vx = R(40); W.man.vy = 0; W.man.gt = 0;
    int seen = 0, f = 0;
    double xin = 0, vxout = 0, yout = 0;
    for(f = 0; f < 30 && !seen; f++){
      mx_update(&W, DT);
      for(int e = 0; e < W.nev; e++) if(W.ev[e].type == EV_TUBE){ seen = 1; xin = D(W.ev[e].x); CHECK(W.ev[e].m == ti, "the event names the tube"); }
      if(seen){ vxout = D(W.man.vx); yout = D(W.man.y); }
    }
    char b[96];
    sprintf(b, "tube %d catches a man flying into the mouth (frame %d)", ti + 1, f); CHECK(seen, b);
    sprintf(b, "tube %d posts him out at the nozzle, flying right at ~95 cells/s (vx %.1f, y %.2f)", ti + 1, vxout, yout);
    CHECK(vxout > 90 && fabs(yout - D(t.by)) < 1.5 && W.man.x > t.bx - R(2), b);
    /* he must not be re-caught by another mouth straight away, and he flies on */
    int again = 0; for(int k = 0; k < 20; k++){ mx_update(&W, DT); for(int e = 0; e < W.nev; e++) if(W.ev[e].type == EV_TUBE) again = 1; }
    CHECK(!again, "no second tube within the cooldown");
    (void)xin;
  }
  /* a man who misses the mouth by more than 3.75 cells is not caught */
  mx_load(&W, 20); for(int i = 0; i < 120; i++) mx_update(&W, DT);
  W.man.mode = MAN_FLY; W.man.x = W.tubes[0].ax - R(5); W.man.y = W.tubes[0].ay - R(6); W.man.vx = R(40); W.man.vy = 0;
  int caught = 0; for(int k = 0; k < 20; k++){ mx_update(&W, DT); for(int e = 0; e < W.nev; e++) if(W.ev[e].type == EV_TUBE && W.ev[e].m == 0) caught = 1; }
  CHECK(!caught, "a man passing 6 cells above the mouth is not caught");

  /* sand: the hourglass level starts with sand in the upper bulb and it must stand until the plug goes */
  mx_load(&W, 21);
  int sand0 = 0; for(int i = 0; i < MX_ROWS * MX_COLS; i++) sand0 += W.grid[i] == MX_SAND;
  for(int i = 0; i < 180; i++) mx_update(&W, DT);
  int sand1 = 0; for(int i = 0; i < MX_ROWS * MX_COLS; i++) sand1 += W.grid[i] == MX_SAND;
  printf("   hourglass sand cells: %d at load, %d after 3 s\n", sand0, sand1);
  CHECK(sand0 > 200 && sand1 == sand0, "the hourglass's sand stays put until something breaks the plug");
  if(fails){ printf("FAILED %d\n", fails); return 1; }
  printf("all tube checks passed\n"); return 0;
}

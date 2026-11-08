/* Air valve + power-up rules: hold on the valve -> inflates, bursts at ~3.6 s and costs a man; let go on the valve -> stays seated;
   drag off -> fires slower with a bigger radius; beans / towel / splat are one use per level. */
#include <stdio.h>
#include <stdlib.h>
#include "mx.h"
static MxWorld W;
#define DT R(0.0166667)
static int fails;
#define CHECK(c, msg) do{ if(!(c)){ printf("FAIL: %s\n", msg); fails++; } else printf("ok: %s\n", msg); }while(0)
static void run(int n){ for(int i = 0; i < n; i++) mx_update(&W, DT); }
int main(void){
  mx_load(&W, 0);
  int men0 = W.men;
  /* 1: hold on the valve until it bursts */
  mx_set_pull(&W, MX_VALVE_X - MX_ANCHOR_X, MX_VALVE_Y - MX_ANCHOR_Y);
  CHECK(W.man.vlv == 1, "dragging onto the valve snaps him there");
  run(60 * 2);
  CHECK(W.man.inf > R(0.4) && W.man.inf < R(0.7), "about 55% inflated after 2 s");
  run(60 * 3);
  CHECK(W.st_pops == 1 && W.men == men0 - 1, "held too long: burst, one man lost");
  run(60 * 2);
  CHECK(W.man.mode == MAN_AIM && W.man.inf == 0, "next man arrives deflated");
  /* 2: let go on the valve: stays seated and keeps filling */
  mx_set_pull(&W, MX_VALVE_X - MX_ANCHOR_X, MX_VALVE_Y - MX_ANCHOR_Y);
  mx_release(&W);
  CHECK(W.man.seated == 1 && W.man.mode == MAN_AIM, "release on the valve = seated, not fired");
  run(60);
  CHECK(W.man.inf > R(0.2), "seated man keeps inflating");
  /* 3: drag him off and fire inflated */
  mx_set_pull(&W, -R(3), R(5));
  CHECK(W.man.vlv == 0 && W.man.seated == 0, "dragging off the valve unseats him");
  mx_release(&W);
  CHECK(W.man.mode == MAN_FLY && W.man.rad > R(1.9), "fires with a bigger radius");
  /* 4: powers are one use per level */
  mx_load(&W, 1);
  mx_select_power(&W, PW_TOWEL);
  CHECK(W.pw_sel == PW_TOWEL, "arm towel");
  mx_set_pull(&W, -R(3), R(5)); mx_release(&W);
  CHECK(W.man.pwr == PW_TOWEL && W.pw_left[PW_TOWEL] == 0 && W.pw_sel == 0, "towel consumed on launch");
  for(int i = 0; i < 60 * 8 && W.man.mode != MAN_AIM; i++) mx_update(&W, DT);
  mx_select_power(&W, PW_TOWEL);
  CHECK(W.pw_sel == 0, "towel can't be armed twice");
  mx_select_power(&W, PW_BEANS);
  mx_set_pull(&W, -R(3), R(5)); mx_release(&W);
  run(20); real vx0 = W.man.vx; mx_dive(&W);
  CHECK(W.man.beaned && W.man.vx > vx0 && !W.man.dive, "beans jet speeds him up instead of diving");
  printf(fails ? "VALVE TEST FAILED\n" : "valve test passed\n");
  return fails != 0;
}

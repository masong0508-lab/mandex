/* BANK SHOT trick shots: game-core mechanics in float and fixed modes.
 * This deliberately tests the real kill/rubber-bounce helpers in mx.c,
 * including collision source, bonus expiry and one award per bounce. */
#include <stdio.h>
#include <string.h>
#include "../core/mx.c"
static MxWorld w;
#define CHECK(expr,msg) do{if(!(expr)){fprintf(stderr,"bank shot FAIL: %s (line %d)\n",msg,__LINE__);return 1;}}while(0)
static void setup(void){
  mx_load(&w, 0);
  w.nev=0; w.state=MX_PLAY; w.score=0;
  w.combo=0;w.combo_left=0;w.bank_left=0;w.bank_contact=0;
  w.man.mode=MAN_FLY;w.man.x=RI(56);w.man.y=RI(40);
  w.ntargets=2;
  for(int i=0;i<2;i++){
    memset(&w.targets[i],0,sizeof w.targets[i]);
    w.targets[i].alive=1;w.targets[i].hp=1;
    w.targets[i].x=w.man.x+RI(i);w.targets[i].y=w.man.y;
  }
}
int main(void){
  setup();
  w.bank_left=RI(2); w.bank_contact=1;
  kill_target(&w,&w.targets[0]);
  w.bank_contact=0;
  CHECK(w.score==6500,"first direct bank-shot award must be 5000+1500");
  CHECK(w.bank_left==0,"bank-shot award not consumed");
  CHECK(w.nev>0&&w.ev[w.nev-1].type==EV_TARGET&&w.ev[w.nev-1].b==RI(1),"bank-shot visual event missing");
  w.bank_contact=1;
  kill_target(&w,&w.targets[1]);
  w.bank_contact=0;
  CHECK(w.score==12500,"same bounce paid again instead of ordinary combo");
  CHECK(w.ev[w.nev-1].b==0,"duplicate trick-shot flag");
  setup();
  w.bank_left=RI(2);                /* nearby indirect rubble/TNT eviction */
  kill_hard(&w,&w.targets[0]);
  CHECK(w.score==5000 && w.bank_left>0 && w.ev[w.nev-1].b==0,
        "indirect eviction incorrectly claimed a bank shot");
  setup();
  w.bank_left=RI(2);combo_tick(&w,RI(3));w.bank_contact=1;
  kill_target(&w,&w.targets[0]);
  CHECK(w.score==5000&&w.bank_left==0,"expired shot paid");
  setup();
  w.targets[0].hp=2;w.bank_left=RI(2);w.bank_contact=1;
  kill_target(&w,&w.targets[0]);
  CHECK(w.targets[0].alive&&w.score==0&&w.bank_left>0,"armored survival consumed bonus");
  w.targets[0].inv=0;
  kill_target(&w,&w.targets[0]);
  CHECK(!w.targets[0].alive&&w.score==6500,"second actual contact should earn bonus");

  setup();
  /* Real collision: a man inside a rubber cell, approaching the surface
     along +X, rebounds in the other direction and arms the timer. */
  int ix=55,iy=40;w.grid[IDX(ix,iy)]=MX_RUBBER;
  w.man.x=RI(ix)+R(0.5);w.man.y=RI(iy)+R(0.5);
  w.man.vx=RI(10);w.man.vy=0;w.man.hitCd=0;
  bounce_rubber(&w);
  CHECK(w.bank_left==RI(2)&&w.man.vx<0,"rubber did not arm bank shot");
  CHECK(w.nev>0&&w.ev[w.nev-1].type==EV_BOUNCE,"bounce event missing");
  next_man(&w);
  CHECK(w.bank_left==0,"bank shot carried into a different projectile");

  puts("bank shot: direct collision, two-second expiry, armor, rubber bounce, one reward per rebound passed");
  return 0;
}

/* Shared 60 Hz event-driven landlord reactions.
   White-box test ensures safety, event selectivity, facing, and the
   allocation-free shared core behavior on both real-number backends. */
#include <stdio.h>
#include <string.h>
#include "../core/mx.c"
static MxWorld w;
static int fails;
#define OK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);fails++;}}while(0)
static void reset(void){
  memset(&w,0,sizeof w); w.state=MX_PLAY; w.ntargets=4;
  MxTarget *a=&w.targets[0], *b=&w.targets[1], *c=&w.targets[2], *d=&w.targets[3];
  a->alive=1; a->kind=LK_WALKER; a->x=RI(50); a->y=RI(30);
  b->alive=1; b->kind=LK_HEALER; b->x=RI(30); b->y=RI(32);
  c->alive=1; c->kind=LK_BOSS; c->x=RI(48); c->y=RI(32);
  d->alive=0; d->kind=LK_SPRINTER; d->x=RI(52); d->y=RI(30);
}
int main(void){
  reset(); ev(&w,EV_CELL,RI(48),RI(30),MX_BRICK,0,0);
  OK(w.nev==1 && w.targets[0].pan==0 && w.targets[0].dodge==0);
  reset(); ev(&w,EV_BOOM,RI(42),RI(30),0,0,0);
#if MX_LORD_SHOCK_REACT
  OK(w.targets[0].pan>0 && w.targets[0].dodge>0 && w.targets[0].dd==1);
  OK(w.targets[1].pan>0 && w.targets[1].dodge>0 && w.targets[1].dd==-1);
  OK(w.targets[2].pan>0 && w.targets[2].dodge==0);
  OK(w.targets[3].pan==0 && w.targets[3].dodge==0);
#else
  OK(w.targets[0].pan==0 && w.targets[0].dodge==0);
#endif
  reset(); ev(&w,EV_THUD,RI(50),RI(30),0,R(0.3),0);
  OK(w.targets[0].dodge==0);
  reset(); ev(&w,EV_CRACK,RI(50),RI(30),0,0,0);
#if MX_LORD_SHOCK_REACT
  OK(w.targets[0].dodge>0 && w.targets[0].pan>0);
  OK(w.targets[1].dodge==0);
#endif
  reset(); w.state=MX_WIN; ev(&w,EV_BOOM,RI(50),RI(30),0,0,0);
  OK(w.targets[0].dodge==0);
  /* Even when all 96 FX slots are exhausted, the world still knows an
     explosion happened. The old early return dropped AI physics alongside
     the decorative effect, making enemy behavior depend on VFX volume. */
  reset(); w.nev=MX_MAX_EVENTS;
  for(int k=0;k<MX_MAX_EVENTS;k++)w.ev[k].type=EV_CELL;
  ev(&w,EV_BOOM,RI(50),RI(30),0,0,0);
  OK(w.nev==MX_MAX_EVENTS && w.ev[MX_MAX_EVENTS-1].type==EV_CELL);
#if MX_LORD_SHOCK_REACT
  OK(w.targets[0].dodge>0 && w.targets[0].pan>0);
  OK(w.targets[2].pan>0 && w.targets[2].dodge==0);
#else
  OK(w.targets[0].dodge==0 && w.targets[0].pan==0);
#endif
  reset(); w.nev=MX_MAX_EVENTS;
  ev(&w,EV_CRACK,RI(50),RI(30),0,0,0);
#if MX_LORD_SHOCK_REACT
  OK(w.targets[0].dodge>0 && w.targets[0].pan>0);
#else
  OK(w.targets[0].dodge==0);
#endif
  reset(); w.nev=MX_MAX_EVENTS;
  ev(&w,EV_THUD,RI(50),RI(30),0,R(0.3),0);
  OK(w.targets[0].dodge==0 && w.nev==MX_MAX_EVENTS);
  ev(&w,EV_THUD,RI(50),RI(30),0,R(0.7),0);
#if MX_LORD_SHOCK_REACT
  OK(w.targets[0].dodge>0 && w.nev==MX_MAX_EVENTS);
#else
  OK(w.targets[0].dodge==0);
#endif
  printf("landlord_shock_test: %s\n",fails?"FAILED":"all passed");
  return fails!=0;
}

/* Shared engine event priorities under a full 96-slot demolition queue.
   Verify that cosmetic debris cannot erase a win/district change,
   survivor, evicted landlord, or player control state notification.
   Compile in BOTH float (Dreamcast/desktop) and 20.12 fixed (DS).
   White-box access exercises exactly the core's production emitter. */
#include <stdio.h>
#include <string.h>
#include "../core/mx.c"

static MxWorld w;
static int fails;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"event overflow FAIL line %d: %s\n",__LINE__,#x);fails++;}}while(0)
static void fill_cosmetic(void){
  memset(&w,0,sizeof w);
  w.state=MX_PLAY;
  w.nev=MX_MAX_EVENTS;
  for(int k=0;k<MX_MAX_EVENTS;k++){
    w.ev[k].type=EV_CELL;
    w.ev[k].x=RI(k);
    w.ev[k].m=MX_BRICK;
  }
}
static int count_type(int ty){
  int count=0;
  for(int i=0;i<w.nev;i++) if(w.ev[i].type==ty)count++;
  return count;
}
int main(void){
  fill_cosmetic();
  /* The old implementation overwrote slot 95 for survivor notifications.
     A later survivor could erase the first, or a district event could be
     silently discarded; both are game-visible bugs. */
  w.ev[7].type=EV_SURVIVOR; w.ev[7].m=1;
  w.ev[MX_MAX_EVENTS-1].type=EV_WIN;
  ev(&w,EV_DISTRICT,RI(70),RI(10),2,0,0);
  CHECK(w.nev==MX_MAX_EVENTS);
  CHECK(w.ev[7].type==EV_SURVIVOR&&w.ev[7].m==1);
  CHECK(w.ev[MX_MAX_EVENTS-2].type==EV_WIN);
  CHECK(w.ev[MX_MAX_EVENTS-1].type==EV_DISTRICT&&w.ev[MX_MAX_EVENTS-1].m==2);
  CHECK(count_type(EV_WIN)==1 && count_type(EV_DISTRICT)==1);
  CHECK(count_type(EV_SURVIVOR)==1);

  ev(&w,EV_SURVIVOR,RI(75),RI(11),2,0,0);
  CHECK(w.ev[MX_MAX_EVENTS-1].type==EV_SURVIVOR && w.ev[MX_MAX_EVENTS-1].m==2);
  CHECK(count_type(EV_SURVIVOR)==2);
  CHECK(count_type(EV_WIN)==1 && count_type(EV_DISTRICT)==1);

  ev(&w,EV_LOSE,RI(76),RI(11),0,0,0);
  CHECK(count_type(EV_LOSE)==1 && count_type(EV_SURVIVOR)==2);
  CHECK(count_type(EV_WIN)==1 && count_type(EV_DISTRICT)==1);

  /* Late objective and input messages likewise take a cosmetic slot. */
  const int important[]={EV_TARGET,EV_LAUNCH,EV_HURT,EV_AIRPOP};
  for(unsigned k=0;k<sizeof important/sizeof important[0];k++){
    ev(&w,important[k],RI(80+k),RI(12),4,RI(k),0);
    CHECK(w.ev[MX_MAX_EVENTS-1].type==important[k]);
    CHECK(count_type(important[k])==1);
  }
  CHECK(count_type(EV_SURVIVOR)==2 && count_type(EV_DISTRICT)==1);
  CHECK(count_type(EV_WIN)==1 && count_type(EV_LOSE)==1);
  /* On full queues, dropped cosmetic events must not displace critical
     records or alter the event count. */
  MxEvent last=w.ev[MX_MAX_EVENTS-1];
  ev(&w,EV_CELL,RI(90),RI(20),MX_WOOD,0,0);
  CHECK(w.nev==MX_MAX_EVENTS && memcmp(&last,&w.ev[MX_MAX_EVENTS-1],sizeof last)==0);

  /* Entire queue critical: stay strictly bounded and keep older control
     signals even if a later notification cannot be represented. */
  memset(&w,0,sizeof w); w.state=MX_PLAY; w.nev=MX_MAX_EVENTS;
  for(int i=0;i<MX_MAX_EVENTS;i++) w.ev[i].type=EV_WIN;
  ev(&w,EV_SURVIVOR,RI(40),RI(30),1,0,0);
  CHECK(w.nev==MX_MAX_EVENTS && count_type(EV_WIN)==MX_MAX_EVENTS);
  CHECK(count_type(EV_SURVIVOR)==0);

  /* Without saturation, the emitter still appends in order. */
  memset(&w,0,sizeof w); w.state=MX_PLAY;
  ev(&w,EV_CELL,RI(7),RI(1),MX_STONE,0,0);
  ev(&w,EV_TARGET,RI(8),RI(2),1,0,0);
  CHECK(w.nev==2 && w.ev[0].type==EV_CELL && w.ev[1].type==EV_TARGET);

  printf("shared event overflow: %s (float/fixed, priorities, order, bounded queue)\n",
         fails?"FAILED":"passed");
  return fails!=0;
}

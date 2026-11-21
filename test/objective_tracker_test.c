#include <stdio.h>
#include <string.h>
#include "mx_objective.h"

static MxWorld w;
static int fails;
#define CHECK(x,msg) do{if(!(x)){fprintf(stderr,"FAIL: %s\n",msg);fails++;}}while(0)
static MxObjective snap(void){return mx_objective_snapshot(&w);}

int main(void){
  memset(&w,0,sizeof w);
  w.state=MX_PLAY; w.level=0; w.ntargets=3; w.men=3;
  w.man.mode=MAN_AIM; w.man.x=RI(20);
  for(int i=0;i<3;i++){w.targets[i].alive=1;w.targets[i].x=RI(40+i*5);}
  MxObjective o=snap();
  CHECK(o.phase==MX_OBJ_EVICT && o.remaining==3 && o.total==3,
        "damage percentage never replaces the actual three-landlord objective");
  w.targets[0].alive=0;w.targets[1].alive=0;
  o=snap();
  CHECK(o.phase==MX_OBJ_LAST && o.remaining==1 && o.direction==1,
        "last living landlord is right of the player");
  w.man.x=RI(70);
  CHECK(snap().direction==-1,"last landlord direction follows the player's position");
  w.man.x=w.targets[2].x;
  CHECK(snap().direction==0,"aligned target has no misleading arrow");
  w.man.mode=MAN_NONE;w.men=0;
  CHECK(snap().phase==MX_OBJ_NO_MEN,
        "no lives remain: do not promise a new shot");
  w.targets[2].alive=0;
  CHECK(snap().phase==MX_OBJ_SECURE,
        "all landlords gone but 1.6 second win confirmation still running");
  w.level=MX_HQ_LEVEL;w.district=0;
  CHECK(snap().phase==MX_OBJ_DISTRICT,"HQ first district will advance");
  w.district=1;
  CHECK(snap().phase==MX_OBJ_DISTRICT,"HQ second district will advance");
  w.district=MX_HQ_DISTRICTS-1;
  CHECK(snap().phase==MX_OBJ_SECURE,"HQ final district will end normally");
  w.state=MX_WIN;
  CHECK(snap().phase==MX_OBJ_COMPLETE,"completed game state overrides settling");
  w.state=MX_LOSE;
  CHECK(snap().phase==MX_OBJ_FAILED,"loss overrides any remaining landlord counts");
  CHECK(strcmp(mx_objective_label(MX_OBJ_LAST),"LAST LANDLORD")==0,
        "shared, understandable short label");
  CHECK(strcmp(mx_objective_label(MX_OBJ_DISTRICT),"NEXT DISTRICT")==0,
        "multi-district stage label");
  if(fails)return 1;
  puts("OBJECTIVE_TRACKER_PASS eviction=1 final_target=1 directions=3 hq_stages=3");
  return 0;
}

/* Native prop animation: exhaustive fractured-pad geometry and actual
 * pneumatic transfer timing; no renderer-only simulation or allocations. */
#include <stdio.h>
#include <string.h>
#include "mx.h"
static MxWorld W;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL prop line %d: %s\n",__LINE__,#c);return 1;}}while(0)
int main(void){
  MxMan clock={0};clock.mode=MAN_WALK;
  clock.wt=R(48.2);CHECK(mx_walk_seconds_left(&clock)==49);
  clock.wt=RI(45);CHECK(mx_walk_seconds_left(&clock)==45);
  clock.wt=R(.01);CHECK(mx_walk_seconds_left(&clock)==1);
  clock.wt=R(-.1);CHECK(mx_walk_seconds_left(&clock)==0);
  clock.wt=RI(100);CHECK(mx_walk_seconds_left(&clock)==60);
  clock.mode=MAN_AIM;CHECK(mx_walk_seconds_left(&clock)==0);
  int cases=0;
  for(int width=1;width<=8;width++)for(unsigned mask=0;mask<(1u<<width);mask++){
    MxPad p={40,20,(uint8_t)width};
    for(int j=0;j<width;j++)W.grid[20*MX_COLS+40+j]=(mask&(1u<<j))?MX_RUBBER:MX_WOOD;
    int best=0,expected=40;
    /* Independent oracle: enumerate candidate contiguous spans. */
    for(int start=0;start<width;start++)for(int end=start;end<width;end++){
      unsigned bits=((1u<<(end-start+1))-1u)<<start;
      if((mask&bits)==bits&&end-start+1>best){best=end-start+1;expected=40+start;}
    }
    int first=-1,n=mx_pad_live_run(&W,&p,&first);
    CHECK(n==best&&first==expected);cases++;
  }
  MxPad edge={118,59,6};W.grid[59*MX_COLS+118]=W.grid[59*MX_COLS+119]=MX_RUBBER;
  int first;CHECK(mx_pad_live_run(&W,&edge,&first)==2&&first==118);
  edge.y=60;CHECK(mx_pad_live_run(&W,&edge,&first)==0);
  edge.y=59;edge.x=255;CHECK(mx_pad_live_run(&W,&edge,&first)==0);
  for(int k=0;k<3;k++){
    CHECK(mx_pad_phase(R(.25),k)==(15+20*k)%60);
    CHECK(mx_pad_phase(R(3600.25),k)==mx_pad_phase(R(.25),k));
  }
  for(int ti=0;ti<3;ti++){
    mx_load(&W,20);CHECK(W.ntubes==3);
    for(int i=0;i<120;i++)mx_update(&W,R(1.0/60.0));
    for(int j=0;j<W.ntubes;j++)CHECK(mx_tube_pulse_stage(&W.tubes[j])==0);
    MxTube t=W.tubes[ti];
    W.man.mode=MAN_FLY;W.man.x=t.ax-R(5);W.man.y=t.ay;
    W.man.vx=R(40);W.man.vy=0;W.man.gt=0;
    int seen=0;
    for(int f=0;f<30&&!seen;f++){
      mx_update(&W,R(1.0/60.0));
      for(int e=0;e<W.nev;e++)if(W.ev[e].type==EV_TUBE&&W.ev[e].m==ti)seen=1;
    }
    CHECK(seen);
    int stage=mx_tube_pulse_stage(&W.tubes[ti]);CHECK(stage>=10&&stage<=12);
    for(int j=0;j<W.ntubes;j++)if(j!=ti)CHECK(mx_tube_pulse_stage(&W.tubes[j])==0);
    W.freeze=60; /* visual countdown continues when physics freezes */
    for(int f=0;f<10;f++)mx_update(&W,R(1.0/60.0));
    int mid=mx_tube_pulse_stage(&W.tubes[ti]);CHECK(mid>0&&mid<stage);
    for(int f=0;f<25;f++)mx_update(&W,R(1.0/60.0));
    CHECK(mx_tube_pulse_stage(&W.tubes[ti])==0);
    mx_load(&W,0);CHECK(W.ntubes==0);
  }
  printf("NATIVE_PROP_ANIMATION_PASS masks=%d edge=1 phase=1 real_tubes=3 expiry=1\n",cases);return 0;
}

/* Exercise real walking updates, including rapid countersteering. The view
 * must move through adjacent rear-atlas samples instead of restarting from
 * the opposite side. Numeric mode must not affect this cosmetic sequence. */
#include <stdio.h>
#include <string.h>
#include "mx.h"

static MxWorld w;
static int failures, samples;
#define CHECK(c,msg) do { if(!(c)){ if(failures<12)fprintf(stderr,"FAIL: %s\n",msg); failures++; } } while(0)

/* Read exactly the profile offset and mirror flag consumed by the three
 * console renderers. 0=right profile, 6=rear, 12=left profile. */
static int drawn_yaw(const MxMan *m){
  int offset=mx_walk_turn_offset(m);
  return mx_walk_render_face(m)<0 ? 12-offset : offset;
}

static void scene(int face,int airborne){
  mx_load(&w,22);
  memset(w.grid,0,sizeof w.grid);
  memset(w.hp,0,sizeof w.hp);
  memset(w.anch,0,sizeof w.anch);
  memset(w.chunks,0,sizeof w.chunks);
  memset(w.targets,0,sizeof w.targets);
  w.ntargets=1;w.targets[0].alive=1;w.targets[0].hp=1;
  w.targets[0].x=RI(100);w.targets[0].y=R(58.5);
  w.npads=w.nrooms=0;w.need_support=w.wob_on=w.freeze=0;
  w.banner=w.slow=0;w.state=MX_PLAY;
  mx_set_fight(&w,0);
  memset(&w.man,0,sizeof w.man);
  w.man.mode=MAN_WALK;w.man.walked=1;w.man.wt=RI(MX_WALK_T);
  w.man.x=RI(60);w.man.y=airborne?RI(25):R(60-2.375);
  w.man.gr=(uint8_t)!airborne;w.man.face=(int8_t)face;
}

static void step(int input,int punch,int *yaw,int *facing){
  if(input)*facing=input;
  const int target=*facing<0?12:0;
  if(*yaw<target)(*yaw)++;
  else if(*yaw>target)(*yaw)--;
  real old_x=w.man.x;
  mx_walk_input(&w,input,0,punch,0);
  mx_update(&w,R(1.0/60.0));
  CHECK(w.man.mode==MAN_WALK,"test scene remains in walking mode");
  CHECK(w.man.face==*facing,"gameplay face responds immediately, including during a turn");
  CHECK(drawn_yaw(&w.man)==*yaw,"rendered yaw stays on the adjacent atlas sample toward the requested side");
  CHECK(w.man.turn_ticks<=11,"remaining turn samples stay within their original budget");
  for(int i=0;i<w.nev;i++)if(w.ev[i].type==EV_PUNCH){
    real expected=old_x+(*facing<0?-R(2.125):R(2.125));
    CHECK(rabs(w.ev[i].x-expected)<R(.001),"punch uses immediate input facing, not delayed visual facing");
    real dx,dy;mx_walk_punch_fx_point(&w.man,0,&dx,&dy);
    CHECK(mx_walk_punch_fx_stage(&w.man)==3,"actual punch starts the full visible sweep");
    CHECK(rabs(old_x+dx-w.ev[i].x-(*facing<0?-MX_WALK_PUNCH_HW:MX_WALK_PUNCH_HW))<R(.001),
          "rendered punch tip agrees with the actual attack event during a turn");
  }
  samples++;
}

int main(void){
  /* Reverse again at EVERY visible point of the original 12-sample arc,
   * including either side of the rear-view mirror swap. Neutral input then
   * finishes the same turn without inventing another direction change. */
  for(int airborne=0;airborne<=1;airborne++)for(int initial=-1;initial<=1;initial+=2){
    for(int elapsed=1;elapsed<=12;elapsed++){
      scene(initial,airborne);
      int yaw=initial<0?12:0,facing=initial;
      for(int i=0;i<elapsed;i++)step(-initial,0,&yaw,&facing);
      step(initial,1,&yaw,&facing);
      for(int i=0;i<12;i++)step(0,0,&yaw,&facing);
      CHECK(w.man.turn_ticks==0 && drawn_yaw(&w.man)==(initial<0?12:0),
            "released countersteer finishes at the requested profile");
    }
    /* Alternating every frame used to snap nearly a half-turn repeatedly.
     * Longer dwell reaches and crosses the rear before each interruption. */
    for(int dwell=1;dwell<=7;dwell++){
      scene(initial,airborne);
      int yaw=initial<0?12:0,facing=initial;
      for(int i=0;i<84;i++)step((i/dwell)&1?initial:-initial,i%19==0,&yaw,&facing);
      for(int i=0;i<12;i++)step(0,0,&yaw,&facing);
      CHECK(w.man.turn_ticks==0,"rapid alternation still settles after input release");
    }
  }
  if(failures){fprintf(stderr,"WALK_TURN_INTERRUPT_FAIL failures=%d samples=%d\n",failures,samples);return 1;}
  printf("WALK_TURN_INTERRUPT_PASS samples=%d directions=2 interrupted_frames=12 grounded_airborne=2 rapid_dwell=7 immediate_punch=1\n",samples);
  return 0;
}

/* Exercise the real walking solver and input API. A stomp survives the
 * impact long enough to boost a rubber pad, and a held button cannot cancel
 * the fresh ascent before the existing pad cooldown expires. */
#include <stdio.h>
#include <string.h>
#include "../core/mx.c"

static MxWorld w;
static int failures;
#define CHECK(c,msg) do { if(!(c)){fprintf(stderr,"FAIL walk pad: %s\n",msg);failures++;} } while(0)
#define DT R(1.0/60.0)

static void scene(int material){
  init_tables();
  memset(&w,0,sizeof w);
  w.state=MX_PLAY;
  for(int x=17;x<=24;x++){
    w.grid[IDX(x,40)]=(uint8_t)material;
    w.hp[IDX(x,40)]=RI(150);
  }
  MxMan *m=&w.man;
  m->mode=MAN_WALK;m->walked=1;m->face=1;m->wt=RI(MX_WALK_T);
  m->x=R(20.5);m->y=RI(40)-WHH-R(.03);m->vy=RI(8);
}

static void tick(void){w.nev=0;walk_step(&w,DT);}
static int events(int type){
  int n=0;for(int i=0;i<w.nev;i++)n+=w.ev[i].type==type;return n;
}
static int pad_boost(void){
  for(int i=0;i<w.nev;i++)if(w.ev[i].type==EV_PAD)return w.ev[i].m;
  return -1;
}

static void boosted_landing(int pressed_now){
  scene(MX_RUBBER);
  if(!pressed_now){w.man.stomp=1;w.man.vy=RI(90);}
  mx_walk_input(&w,0,0,0,1);
  tick();
  CHECK(events(EV_SLAM)==1,"stomp must make exactly one real landing impact");
  CHECK(events(EV_PAD)==1,"landing must emit exactly one pad launch");
  CHECK(pad_boost()==1,"pad event lost the stomp-boosted flag");
  CHECK(w.man.vy==R(-67.5),"stomp did not use the stronger pad launch");
  CHECK(!w.man.gr&&!w.man.stomp,"launch must finish the old grounded stomp");
  CHECK(w.man.padCd==R(.3),"launch must keep the existing pad cooldown");
  for(int x=17;x<=24;x++)
    CHECK(w.grid[IDX(x,40)]==MX_RUBBER&&w.hp[IDX(x,40)]==RI(150),
          "stomp must not destroy the rubber launch surface");

  /* Keep STOMP held through the launch, as a real keyboard or touch input
   * does. The previous code replaces the upward impulse with +90 next tick. */
  real landing_y=w.man.y;
  for(int i=0;i<8;i++){
    tick();
    CHECK(!w.man.stomp&&w.man.vy<0,"held stomp cancelled a fresh pad ascent");
    CHECK(w.man.y<landing_y,"pad launch failed to carry the walker upward");
    CHECK(events(EV_PAD)==0&&events(EV_SLAM)==0,
          "held button repeated a launch or slam during the ascent");
  }
}

int main(void){
  scene(MX_RUBBER);
  tick();
  CHECK(events(EV_PAD)==1&&pad_boost()==0,"ordinary landing changed pad feedback");
  CHECK(w.man.vy==R(-58.75),"ordinary landing changed normal bounce strength");
  CHECK(!w.man.gr&&!w.man.stomp,"ordinary launch must remain airborne");

  boosted_landing(0);
  boosted_landing(1);

  /* The guard protects ascent only. Expiry returns full stomp control even
   * while the key is still held; a ceiling stopping ascent does so earlier. */
  scene(MX_RUBBER);
  w.man.y=RI(20);w.man.vy=-RI(40);w.man.padCd=R(.005);
  mx_walk_input(&w,0,0,0,1);
  tick();
  CHECK(w.man.vy<0&&!w.man.stomp&&w.man.padCd<=0,
        "the final cooldown tick must still protect its upward launch");
  tick();
  CHECK(w.man.stomp&&w.man.vy>=RI(90),"stomp did not rearm after pad cooldown");

  scene(MX_RUBBER);
  w.man.y=RI(20);w.man.vy=0;w.man.padCd=R(.2);
  mx_walk_input(&w,0,0,0,1);tick();
  CHECK(w.man.stomp&&w.man.vy>=RI(90),"ceiling-stopped ascent kept stomp locked");

  scene(MX_RUBBER);
  w.man.y=RI(20);w.man.vy=RI(10);w.man.padCd=R(.2);
  mx_walk_input(&w,0,0,0,1);tick();
  CHECK(w.man.stomp&&w.man.vy>=RI(90),"falling during cooldown kept stomp locked");

  scene(MX_RUBBER);
  w.man.y=RI(20);w.man.vy=-RI(40);w.man.padCd=0;
  mx_walk_input(&w,0,0,0,1);tick();
  CHECK(w.man.stomp&&w.man.vy>=RI(90),"ordinary airborne stomp was delayed");

  scene(MX_RUBBER);
  w.man.stomp=1;w.man.vy=RI(90);w.man.padCd=R(.2);
  tick();
  CHECK(events(EV_SLAM)==1&&events(EV_PAD)==0,
        "cooldown allowed a repeated pad launch on landing");
  CHECK(w.man.gr&&!w.man.stomp&&w.man.vy==0,
        "cooldown landing did not finish the existing stomp");

  scene(MX_STEEL);
  w.man.stomp=1;w.man.vy=RI(90);tick();
  CHECK(events(EV_SLAM)==1&&events(EV_PAD)==0,"ordinary hard landing lost its slam");
  CHECK(w.man.gr&&!w.man.stomp&&w.man.vy==0,"ordinary hard landing changed response");
  CHECK(w.hp[IDX(20,40)]<RI(150),"ordinary stomp no longer damages the floor");

  if(failures){fprintf(stderr,"WALK_PAD_STOMP_FAIL failures=%d\n",failures);return 1;}
  puts("WALK_PAD_STOMP_PASS normal_pad=1 stomp_boost=1 held_ascent=1 cooldown_rearm=1 ceiling_rearm=1 ordinary_slam=1");
  return 0;
}

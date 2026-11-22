/* Actual cross-port walking solver: short-hop release, edge forgiveness,
   no extra midair jumps, and unchanged stomp/rubber launch behavior. */
#include <stdio.h>
#include <string.h>
#include "../core/mx.c"
static MxWorld w;
static int failures, checks;
#define DT R(1.0/60.0)
#define CHECK(c,msg) do{ checks++; if(!(c)){fprintf(stderr,"FAIL jump: %s\n",msg);failures++;} }while(0)
static void scene(int mat){
  init_tables();
  memset(&w,0,sizeof w);w.state=MX_PLAY;
  if(mat)for(int x=12;x<=35;x++){
    w.grid[IDX(x,40)]=(uint8_t)mat;
    w.hp[IDX(x,40)]=RI(150);
  }
  MxMan *m=&w.man;
  m->mode=MAN_WALK;m->walked=1;m->face=1;m->wt=RI(MX_WALK_T);
  m->x=RI(20);m->y=RI(40)-WHH-R(.03);
  m->gr=mat!=0;m->padCd=0;
}
static void tick(int ax,int jump,int stomp){
  mx_walk_input(&w,ax,jump,0,stomp);
  w.nev=0;walk_step(&w,DT);
}
static real apex(int held_ticks){
  scene(MX_STEEL);real miny=w.man.y;
  for(int i=0;i<50;i++){
    tick(0,i<held_ticks,0);
    if(w.man.y<miny)miny=w.man.y;
  }
  return miny;
}
int main(void){
  scene(MX_STEEL);
  real y0=w.man.y;
  tick(0,1,0);
  CHECK(w.man.vy<0&&!w.man.gr,"ground jump begins immediately");
  CHECK(w.man.jump_hold>0&&w.man.ledge_grace==0,"jump initializes only its hold window");
  tick(0,0,0);
  CHECK(w.man.vy>=R(-22.5)&&w.man.vy<0,"releasing jump cuts upward velocity");
  CHECK(w.man.jump_hold==0,"early release is a single bounded cut");
  CHECK(w.man.y<y0,"tap jump leaves the floor");
  real short_apex=apex(1),high_apex=apex(20);
  CHECK(high_apex<short_apex-R(3),"holding jump reaches a meaningfully higher apex");

  scene(MX_STEEL);
  tick(0,1,0);
  real v0=w.man.vy;
  for(int i=0;i<5;i++)tick(0,1,0);
  CHECK(w.man.vy>v0&&w.man.vy<0,
       "held jump uses the original gravity and never restarts in midair");
  CHECK(w.man.ledge_grace==0,"ordinary jump cannot preserve a second grace jump");

  scene(0);
  w.man.ledge_grace=R(.06);w.man.vy=RI(8);
  tick(0,1,0);
  CHECK(w.man.vy<0&&w.man.jump_hold>0&&w.man.ledge_grace==0,
        "airborne walker can jump during ledge grace");
  real first=w.man.vy;
  tick(0,1,0);
  CHECK(w.man.vy>first,"held coyote jump cannot restart again next frame");

  scene(0);w.man.ledge_grace=R(.02);w.man.vy=RI(4);
  tick(0,0,0);tick(0,0,0);tick(0,1,0);
  CHECK(w.man.ledge_grace==0&&w.man.vy>0,
        "expired grace must not enable an airborne double jump");

  scene(MX_STEEL);
  /* Walk over a real grid ledge, then press jump after ground contact ends. */
  for(int x=20;x<=35;x++)w.grid[IDX(x,40)]=MX_EMPTY;
  w.man.x=R(18.5);
  int fell=0;
  for(int i=0;i<30;i++){
    tick(1,0,0);
    if(!w.man.gr){fell=1;break;}
  }
  CHECK(fell&&w.man.ledge_grace>0,"walking off a real ledge creates short grace");
  tick(0,1,0);
  CHECK(w.man.vy<0&&w.man.ledge_grace==0,
        "jump after the actual ledge edge is permitted");

  scene(MX_STEEL);
  tick(0,1,0);tick(0,0,1);
  CHECK(w.man.stomp&&w.man.vy>=RI(90)&&w.man.jump_hold==0,
        "stomp overrides the short-hop cut immediately");

  scene(MX_RUBBER);
  w.man.gr=0;w.man.vy=RI(8);
  tick(0,0,0);
  CHECK(w.man.vy==R(-58.75)&&w.man.jump_hold==0&&w.man.ledge_grace==0,
        "ordinary pad rebound is unmodified and does not arm jump cutting");
  tick(0,0,0);
  CHECK(w.man.vy<R(-22.5),"releasing jump cannot shorten a bounce pad launch");

  if(failures){fprintf(stderr,"WALK_JUMP_FORGIVENESS_FAIL %d/%d\n",failures,checks);return 1;}
  printf("WALK_JUMP_FORGIVENESS_PASS cases=%d short_hop=1 held_height=1 coyote=1 no_double=1 edge=1 stomp=1 pad=1\n",checks);
  return 0;
}

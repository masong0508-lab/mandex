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
  tick(0,1,0);w.man.y=RI(20);tick(0,0,1);
  CHECK(w.man.stomp&&w.man.vy>=RI(90)&&w.man.jump_hold==0,
        "stomp overrides the short-hop cut immediately");

  scene(MX_RUBBER);
  w.man.gr=0;w.man.vy=RI(8);
  tick(0,0,0);
  CHECK(w.man.vy==R(-58.75)&&w.man.jump_hold==0&&w.man.ledge_grace==0,
        "ordinary pad rebound is unmodified and does not arm jump cutting");
  tick(0,0,0);
  CHECK(w.man.vy<R(-22.5),"releasing jump cannot shorten a bounce pad launch");

  /* Brief tap just before touching down must be consumed once at
     the next grounded tick. No held auto-repeat and no airborne reset. */
  scene(MX_STEEL);w.man.gr=0;w.man.y-=R(.20);w.man.vy=RI(12);
  tick(0,1,0);CHECK(w.man.jump_buffer>0,"pre-landing jump is buffered");
  tick(0,0,0);CHECK(w.man.vy<0&&w.man.jump_buffer==0,"released buffered jump launches once");
  scene(MX_STEEL);tick(0,1,0);
  for(int n=0;n<120;n++)tick(0,1,0);
  CHECK(w.man.gr&&w.man.vy==0,"holding jump after landing never auto-repeats");
  /* Recovery from a thick rubble box must not leave the player stuck.
     The old safe point is on a separate clear platform. */
  scene(MX_STEEL);w.man.walk_safe=1;w.man.walk_safe_x=RI(15);w.man.walk_safe_y=RI(40)-WHH-R(.03);
  w.man.x=RI(80);w.man.y=RI(30);
  for(int y=15;y<45;y++)for(int x=65;x<96;x++)w.grid[IDX(x,y)]=MX_STEEL;
  for(int n=0;n<20;n++)tick(0,0,0);
  CHECK(w.man.x<RI(30)&&!wblocked(&w,w.man.x,w.man.y),"wedged walker returns to validated safe point");
  scene(0);w.ntubes=1;w.tubes[0]=(MxTube){.ax=w.man.x,.ay=w.man.y,.bx=RI(70),.by=RI(20),.vx=RI(60)};
  real timer=w.man.wt;walk_tube_step(&w,DT);
  CHECK(w.man.mode==MAN_WALK&&w.man.x==RI(70)&&w.man.y==RI(20)&&w.man.vx==RI(22)&&w.man.vy==R(-18.75)&&w.man.wt==timer,"tube crosses safely on foot with no lost life or time");
  CHECK(w.man.tubeCd>R(.8)&&w.tubes[0].pulse>0,"tube cooldown and visual pulse");
  scene(0);w.ntubes=1;w.tubes[0]=(MxTube){.ax=w.man.x,.ay=w.man.y,.bx=RI(70),.by=RI(20),.vx=RI(60)};
  for(int y=10;y<=30;y++)for(int x=65;x<=75;x++)w.grid[IDX(x,y)]=MX_STEEL;
  real ox=w.man.x,oy=w.man.y;walk_tube_step(&w,DT);
  CHECK(w.man.x==ox&&w.man.y==oy&&w.man.tubeCd>0,"solid tube exit refuses transport");
  /* Exhaust the sample boundaries against the old Cartesian predicate.
     Live, dead and sleeping cells share the same voxel geometry. */
  scene(0);
  MxChunk *ch=&w.chunks[0];
  ch->active=1;ch->rest=1;ch->count=1;ch->rad=RI(20);ch->x=RI(40);ch->y=RI(30);
  MxCell *a=&ch->cells[0];
  const real xs[3]={ch->x-WHW,ch->x,ch->x+WHW};
  const real ys[5]={ch->y-WHH,ch->y-R(1.1875),ch->y,ch->y+R(1.1875),ch->y+WHH-R(.0125)};
  for(int ix=-16;ix<=16;ix++)for(int iy=-28;iy<=28;iy++){
    a->wx=ch->x+rmul(RI(ix),R(.125));
    a->wy=ch->y+rmul(RI(iy),R(.125));
    int expect=0;
    for(int x=0;x<3;x++)for(int y=0;y<5;y++)
      if(rabs(a->wx-xs[x])<RC&&rabs(a->wy-ys[y])<RC)expect=1;
    CHECK(wblocked(&w,ch->x,ch->y)==expect,"separable voxel samples preserve original collision");
  }
  a->wx=ch->x;a->wy=ch->y;a->dead=1;
  CHECK(!wblocked(&w,ch->x,ch->y),"destroyed debris does not block walking");
  /* At the ceiling the old local pop-out could put the body above the map.
     A jammed walker also must not trigger a tube until safely recovered. */
  scene(0);w.man.x=RI(2);w.man.y=WHH;
  w.man.walk_safe=1;w.man.walk_safe_x=RI(25);w.man.walk_safe_y=RI(20);
  for(int y=0;y<10;y++)for(int x=0;x<15;x++)w.grid[IDX(x,y)]=MX_STEEL;
  w.ntubes=1;w.tubes[0]=(MxTube){.ax=w.man.x,.ay=w.man.y,.bx=RI(70),.by=RI(20),.vx=RI(60)};
  tick(0,0,0);
  CHECK(w.man.y==WHH&&w.man.x==RI(2)&&w.man.walk_jam>0,"ceiling jam refuses off-map escape");
  walk_tube_step(&w,DT);
  CHECK(w.man.x==RI(2)&&w.man.tubeCd==0,"jammed walker cannot activate tube");
  for(int n=0;n<20;n++)tick(0,0,0);
  CHECK(w.man.x==RI(25)&&w.man.y>=WHH&&!wblocked(&w,w.man.x,w.man.y),"ceiling jam restores valid safe point");
  if(failures){fprintf(stderr,"WALK_JUMP_FORGIVENESS_FAIL %d/%d\n",failures,checks);return 1;}
  printf("WALK_JUMP_FORGIVENESS_PASS cases=%d short_hop=1 held_height=1 coyote=1 no_double=1 edge=1 stomp=1 pad=1\n",checks);
  return 0;
}

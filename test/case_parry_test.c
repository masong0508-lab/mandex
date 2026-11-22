/* Tests real shared walking hitboxes: a punch or belly slam may parry an
 * incoming case, but misses must not award points, effects or invulnerability.
 * Compile in both float and fixed point, without a second core/mx.c. */
#include <stdio.h>
#include <string.h>
#include "../core/mx.c"
static MxWorld w;
static int checks,failures;
#define CHECK(x,msg) do{checks++;if(!(x)){fprintf(stderr,"FAIL case parry: %s\n",msg);failures++;}}while(0)
#define DT R(1.0/60.0)
static int events(int type){
  int n=0;for(int i=0;i<w.nev;i++) n+=(w.ev[i].type==type);return n;
}
static void setup(void){
  init_tables();memset(&w,0,sizeof w);w.state=MX_PLAY;w.walk=1;w.men=3;w.nofight=1;
  w.ntargets=1;w.targets[0].alive=1;w.targets[0].x=RI(100);w.targets[0].y=RI(40);
  w.man.mode=MAN_WALK;w.man.face=1;w.man.wt=RI(30);w.man.x=RI(40);
  w.man.y=R(57.625);w.man.gr=1;
}
static void case_at(int i,real x,real y){
  MxCase *c=&w.cases[i];c->active=1;c->x=x;c->y=y;
  c->vx=-RI(2);c->vy=0;c->rot=0;c->l=RI(4);
}
static real punch_x(void){return w.man.x+R(2.125);}
static real punch_y(void){return w.man.y-R(.375);}
static void expect_parry(int want){
  CHECK(events(EV_BONK)==want,"missing, extra or duplicate parry event");
  for(int i=0;i<w.nev;i++)if(w.ev[i].type==EV_BONK)
    CHECK(w.ev[i].m==2,"parry event must be distinct from bat/shield BONK");
}
int main(void){
  setup();case_at(0,punch_x(),punch_y());
  mx_walk_input(&w,0,0,1,0);mx_update(&w,DT);
  CHECK(!w.cases[0].active,"punch must remove incoming briefcase before its contact step");
  CHECK(w.score==150,"one parried case earns a bounded 150-point bonus");
  CHECK(events(EV_PUNCH)==1&&events(EV_HURT)==0&&events(EV_THUD)==0,
        "parried projectile cannot hurt or shatter again in this frame");
  expect_parry(1);
  CHECK(w.man.hurtCd==0&&w.man.mode==MAN_WALK,"successful parry preserved the walker");

  setup();case_at(0,w.man.x-R(3),punch_y());
  walk_punch(&w);
  CHECK(w.cases[0].active&&w.score==0,"punch may not parry a case behind the player");
  expect_parry(0);

  setup();case_at(0,punch_x()+R(2.375),punch_y());
  walk_punch(&w);
  CHECK(w.cases[0].active&&w.score==0,"case at the exact outer punch boundary is a miss");
  expect_parry(0);

  setup();case_at(0,punch_x(),punch_y()+RI(4));
  walk_punch(&w);
  CHECK(w.cases[0].active&&w.score==0,"out-of-height case cannot be punched");
  expect_parry(0);

  setup();case_at(0,punch_x(),punch_y());case_at(1,punch_x()+R(.5),punch_y());
  walk_punch(&w);
  CHECK(!w.cases[0].active&&!w.cases[1].active,"one punch can cover two overlapping cases");
  CHECK(w.score==300,"each destroyed case scores once, not once per physics substep");
  expect_parry(2);
  w.nev=0;walk_punch(&w);
  CHECK(w.score==300&&events(EV_BONK)==0,"inactive cases cannot be farmed for points");

  setup();case_at(0,w.man.x,w.man.y+WHH+RI(1));
  walk_slam(&w);
  CHECK(!w.cases[0].active&&w.score==150,
        "a descending belly slam parries the case only once across its two hitboxes");
  CHECK(events(EV_SLAM)==1,"belly slam retained its original impact event");
  expect_parry(1);

  setup();walk_punch(&w);
  CHECK(w.score==0&&events(EV_BONK)==0&&events(EV_PUNCH)==1,
        "ordinary punch without a case is unchanged");

  setup();case_at(0,w.man.x,w.man.y);w.nofight=0;
  mx_walk_input(&w,0,0,0,0);mx_update(&w,DT);
  CHECK(events(EV_BONK)==0&&w.score==0,
        "no parry occurs without pressing the attack button");
  CHECK(events(EV_HURT)==1,
        "normal incoming briefcase still hurts an undefended walker");

  if(failures){fprintf(stderr,"CASE_PARRY_FAIL %d/%d\n",failures,checks);return 1;}
  printf("CASE_PARRY_PASS checks=%d hit=1 forward_only=1 range=1 multiple=1 slam=1 points=1 vulnerable=1\n",checks);
  return 0;
}

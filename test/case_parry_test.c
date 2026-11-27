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
  { int flagged=0;for(int i=0;i<w.nev;i++)
      if(w.ev[i].type==EV_PUNCH)flagged=(w.ev[i].b>0);
    CHECK(flagged,"parried punch must suppress only its generic POW popup"); }
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

  /* Match the browser's 39px offset, testing actual forward momentum. */
  for(int face=-1;face<=1;face+=2){
    for(int speed=-15;speed<=15;speed+=15){
      setup();w.man.face=face;w.man.vx=RI(face*speed);
      case_at(0,w.man.x+face*R(4.875),punch_y());
      walk_punch(&w);
      CHECK(w.score==(speed==15?150:0),"shoulder reach requires forward momentum");
      CHECK(w.cases[0].active==(speed!=15),"charge contact matches facing");
    }
  }

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
  { int flagged=0;for(int i=0;i<w.nev;i++)
      if(w.ev[i].type==EV_SLAM)flagged=(w.ev[i].b>0);
    CHECK(flagged,"parried slam must suppress overlapping generic text"); }
  expect_parry(1);

  setup();walk_punch(&w);
  CHECK(w.score==0&&events(EV_BONK)==0&&events(EV_PUNCH)==1,
        "ordinary punch without a case is unchanged");
  { int generic=0;for(int i=0;i<w.nev;i++)
      if(w.ev[i].type==EV_PUNCH)generic=(w.ev[i].b==0);
    CHECK(generic,"ordinary empty punch retains normal popup priority"); }

  /* Timing window: a briefcase can arrive on a later animation frame.
     Start outside the fist and move inside during the projectile step. */
  setup();w.man.pT=R(.15);
  case_at(0,punch_x()+R(2.875),punch_y());w.cases[0].vx=-RI(80);
  upd_cases(&w,DT);
  CHECK(!w.cases[0].active&&w.score==150,
        "briefcase entering a visible punch arc must be parried");
  expect_parry(1);
  CHECK(events(EV_HURT)==0,"animation-window parry cannot hurt the player");

  /* Charge reach is latched when attacking, not recomputed from speed. */
  for(int face=-1;face<=1;face+=2){
    for(int charged=0;charged<=1;charged++){
      setup();w.man.face=face;w.man.vx=RI(face*(charged?15:0));
      walk_punch(&w);w.man.vx=0;
      case_at(0,w.man.x+face*R(4.875),punch_y());
      w.cases[0].vx=0;upd_cases(&w,DT);
      CHECK(w.score==(charged?150:0),"active charge keeps wider parry reach");
      CHECK(w.cases[0].active==!charged,"ordinary active punch keeps normal reach");
    }
  }
  setup();w.man.vx=RI(15);walk_punch(&w);w.man.pT=0;
  case_at(0,w.man.x+R(4.875),punch_y());w.cases[0].vx=0;
  upd_cases(&w,DT);
  CHECK(w.score==0&&w.cases[0].active,"expired charge cannot parry");

  setup();w.man.pT=0;case_at(0,punch_x(),punch_y());
  upd_cases(&w,DT);
  CHECK(w.score==0&&events(EV_BONK)==0&&events(EV_HURT)==1,
        "expired punch cannot grant a free automatic shield");

  setup();w.man.pT=R(.15);case_at(0,w.man.x-R(3),punch_y());
  upd_cases(&w,DT);
  CHECK(w.cases[0].active&&w.score==0&&events(EV_BONK)==0,
        "back-facing case cannot be parried during fist recovery");

  setup();case_at(0,w.man.x,w.man.y);w.nofight=0;
  mx_walk_input(&w,0,0,0,0);mx_update(&w,DT);
  CHECK(events(EV_BONK)==0&&w.score==0,
        "no parry occurs without pressing the attack button");
  CHECK(events(EV_HURT)==1,
        "normal incoming briefcase still hurts an undefended walker");

  /* Reward every real parry, but cap time at six seconds per life. */
  setup();
  for(int i=0;i<8;i++){
    w.nev=0;case_at(0,punch_x(),punch_y());walk_punch(&w);
    CHECK(w.score==(i+1)*150,"score remains +150 after time budget expires");
    CHECK(w.man.parry_time_used==(i<6?i+1:6),"bounded per-life budget");
    CHECK(w.man.wt==RI(30+(i<6?i+1:6)),"exact clock reward");
    for(int e=0;e<w.nev;e++)if(w.ev[e].type==EV_BONK)
      CHECK(w.ev[e].a==RI(i<6?1:0),"event reports actual time");
    parry_case(&w,&w.cases[0]);
    CHECK(w.score==(i+1)*150,"inactive case cannot score twice");
  }
  setup();w.man.wt=R(59.5);case_at(0,punch_x(),punch_y());walk_punch(&w);
  CHECK(w.man.wt==R(59.5)&&w.man.parry_time_used==0,"near-cap does not waste budget");
  w.man.wt=RI(59);case_at(0,punch_x(),punch_y());walk_punch(&w);
  CHECK(w.man.wt==RI(60)&&w.man.parry_time_used==1,"exact cap");
  w.man.wt=0;case_at(0,punch_x(),punch_y());walk_punch(&w);
  CHECK(w.man.wt==0,"expired clock cannot revive");
  w.man.parry_time_used=6;w.man.mode=MAN_FLY;enter_walk(&w);
  CHECK(w.man.parry_time_used==0&&w.man.wt==RI(MX_WALK_T),"new walking life resets its budget");

  /* Campaign reachability: a nearby thrower on a walking floor must
     telegraph first, then release a real solved arc. */
  setup();w.nofight=0;w.man.mode=MAN_WALK;w.man.x=RI(40);w.man.y=RI(35);
  MxTarget *t=&w.targets[0];t->kind=LK_THROWER;t->hp=t->hpmax=1;
  t->x=RI(55);t->y=RI(35);t->hx=t->x;t->face=-1;t->cd=0;t->wind=0;
  lord_ai(&w,t,DT);
  CHECK(t->wind>0&&events(EV_THROW)==0,
        "walking thrower must visibly wind up before releasing a case");
  for(int f=0;f<50&&events(EV_THROW)==0;f++)lord_ai(&w,t,DT);
  CHECK(events(EV_THROW)==1,
        "nearby thrower must actually attack a walking player");
  int active=0;for(int i=0;i<MX_MAX_CASES;i++)active+=w.cases[i].active;
  CHECK(active==1,"walking thrower must use the existing four-case pool");
  CHECK(t->cd>0,"walking thrower must respect normal attack cooldown");

  setup();w.nofight=0;w.man.mode=MAN_WALK;w.man.x=RI(10);w.man.y=RI(35);
  t=&w.targets[0];t->kind=LK_THROWER;t->x=RI(55);t->y=RI(35);t->cd=0;
  lord_ai(&w,t,DT);
  CHECK(t->wind==0&&events(EV_THROW)==0,
        "walking throwers must not snipe across the map");

  setup();w.nofight=0;w.man.mode=MAN_WALK;w.man.x=RI(40);w.man.y=RI(10);
  t=&w.targets[0];t->kind=LK_THROWER;t->x=RI(55);t->y=RI(35);t->cd=0;
  lord_ai(&w,t,DT);
  CHECK(t->wind==0,"walking throwers must not target far-separated floors");

  setup();w.nofight=1;w.man.mode=MAN_WALK;w.man.x=RI(40);w.man.y=RI(35);
  t=&w.targets[0];t->kind=LK_THROWER;t->x=RI(55);t->y=RI(35);t->cd=0;
  lord_ai(&w,t,DT);
  CHECK(t->wind==0&&events(EV_THROW)==0,"fight-back-off must remain safe");

  mx_load(&w,37);
  int walkers=0,throwers=0;
  for(int i=0;i<w.ntargets;i++){
    walkers+=w.targets[i].kind==LK_WALKER;
    throwers+=w.targets[i].kind==LK_THROWER;
  }
  CHECK(w.walk&&w.ntargets==8&&walkers==3&&throwers==1,
        "Rent Strike must teach parrying without losing a landlord");

  if(failures){fprintf(stderr,"CASE_PARRY_FAIL %d/%d\n",failures,checks);return 1;}
  printf("CASE_PARRY_PASS checks=%d hit=1 forward_only=1 range=1 multiple=1 slam=1 points=1 vulnerable=1\n",checks);
  return 0;
}

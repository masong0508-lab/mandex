/* Actual shared combat paths: a hit grants its full recovery window,
 * including the final HP, and an interrupted enemy cannot finish a windup. */
#include <stdio.h>
#include <string.h>
#include "../core/mx.c"

static MxWorld w;
static int failures, checks;
#define CHECK(c,msg) do { checks++; if(!(c)){ \
  fprintf(stderr,"FAIL combat recovery: %s\n",msg);failures++; } } while(0)

static MxTarget *scene(int kind,int hp){
  init_tables();memset(&w,0,sizeof w);
  w.state=MX_PLAY;w.walk=1;w.ntargets=1;w.men=3;
  w.bh_lo=0;w.bh_hi=MX_ROWS+3;
  w.man.mode=MAN_WALK;w.man.x=RI(40);w.man.y=R(57.625);
  w.man.wt=RI(30);w.man.face=1;w.man.gr=1;
  MxTarget *t=&w.targets[0];
  t->alive=1;t->hp=t->hpmax=(uint8_t)hp;t->kind=(uint8_t)kind;
  t->x=RI(42);t->y=R(57.625);t->hx=t->x;t->face=-1;
  t->range=RI(7);t->cover=-RI(1);
  return t;
}
static int events(int type){
  int count=0;for(int i=0;i<w.nev;i++)count+=w.ev[i].type==type;return count;
}

int main(void){
  /* Repeated contacts, including all eight flight substeps, must never
   * turn the penultimate soft hit into a same-frame eviction. */
  for(int hp=2;hp<=3;hp++){
    MxTarget *t=scene(LK_BOSS,hp);
    for(int hit=1;hit<hp;hit++){
      t->inv=0;kill_target(&w,t);
      CHECK(t->alive&&t->hp==hp-hit,"one soft hit removes exactly one HP");
      CHECK(t->inv>0&&t->stun>0,"a surviving hit grants its recovery timers");
      real inv=t->inv,stun=t->stun,clock=w.man.wt;
      int emitted=w.nev;
      for(int sub=0;sub<16;sub++)kill_target(&w,t);
      CHECK(t->alive&&t->hp==hp-hit,"repeat contacts bypassed final-HP immunity");
      CHECK(t->inv==inv&&t->stun==stun,"blocked hits restarted recovery timers");
      CHECK(w.nev==emitted&&w.score==0&&w.combo==0&&w.man.wt==clock,
            "blocked hits granted eviction feedback, points or walking time");
    }
    t->inv=0;kill_target(&w,t);
    CHECK(!t->alive&&w.score==5000&&events(EV_TARGET)==1,
          "the final hit after recovery must evict and score once");
  }
  {
    MxTarget *t=scene(LK_NONE,1);t->inv=R(.1);
    kill_target(&w,t);
    CHECK(!t->alive&&w.score==5000,
          "ordinary one-HP targets must remain vulnerable during environmental stagger");
    t->inv=0;kill_target(&w,t);
    CHECK(w.score==5000&&events(EV_TARGET)==1,"ordinary one-HP eviction must still score once");
  }
  {
    MxTarget *t=scene(LK_BOSS,2);t->hpmax=0;t->inv=R(.1);
    kill_target(&w,t);
    CHECK(t->alive&&t->hp==2,"legacy multi-HP fixtures retain their original immunity");
  }
  {
    MxTarget *t=scene(LK_BOSS,3);t->inv=RI(1);
    kill_hard(&w,t);
    CHECK(!t->alive&&w.score==5000&&events(EV_TARGET)==1,
          "deliberate hard damage must bypass HP and immunity");
    kill_hard(&w,t);kill_target(&w,t);
    CHECK(w.score==5000&&events(EV_TARGET)==1,"dead targets must never score twice");
  }

  /* The public update processes a real punch, then body overlap in the
   * same walking tick. This is where two-HP landlords previously vanished. */
  {
    MxTarget *t=scene(LK_NONE,2);w.nofight=1;
    mx_walk_input(&w,0,0,1,0);mx_update(&w,R(1.0/60.0));
    CHECK(events(EV_PUNCH)==1&&events(EV_OOF)==1,"real input must land a soft punch");
    CHECK(t->alive&&t->hp==1&&events(EV_TARGET)==0&&w.score==0,
          "same-frame body overlap consumed the last HP after a punch");
    CHECK(w.man.wt<RI(30),"a nonlethal punch must not award eviction time");
  }
  /* A timely punch interrupts the enemy's imminent swing. Without the
   * action guard, the visibly stunned enemy still knocks the player away. */
  {
    MxTarget *t=scene(LK_WALKER,3);t->wind=R(.01);t->cd=RI(2);
    mx_walk_input(&w,0,0,1,0);mx_update(&w,R(1.0/60.0));
    CHECK(t->alive&&t->hp==2&&t->stun>0,"counterpunch must wound and stun the walker");
    CHECK(events(EV_HURT)==0&&w.man.hurtCd==0&&w.man.wt>RI(29),
          "a stunned walker completed its queued blow after the counterpunch");
    CHECK(t->wind==0&&t->mv==0,"stun must cancel the queued attack and active movement");
  }

  for(int kind=LK_THROWER;kind<=LK_BOSS;kind++){
    if(kind!=LK_THROWER&&kind!=LK_BOSS)continue;
    MxTarget *t=scene(kind,3);
    w.man.mode=MAN_AIM;w.man.x=RI(16);w.man.y=RI(45);
    t->x=RI(40);t->y=RI(45);t->stun=R(.5);t->wind=R(.01);
    t->b2=1;t->cd=RI(3);t->swing=R(.3);t->mv=2;
    lord_ai(&w,t,R(.02));
    CHECK(events(EV_THROW)==0,"stunned thrower or boss released a queued briefcase");
    int active=0;for(int i=0;i<MX_MAX_CASES;i++)active+=w.cases[i].active;
    CHECK(active==0&&t->wind==0&&!t->b2&&t->mv==0,
          "stun must cancel the queued throw and boss follow-up");
    CHECK(t->cd==RI(3)&&t->swing>0&&t->swing<R(.3),
          "stun pauses attack cooldown but keeps existing feedback animation");
    t->stun=0;t->cd=0;w.nev=0;
    lord_ai(&w,t,R(.02));
    CHECK(t->wind>0&&events(EV_THROW)==0,"recovered thrower needs a fresh telegraph");
    for(int frame=0;frame<50&&!events(EV_THROW);frame++)lord_ai(&w,t,R(.02));
    CHECK(events(EV_THROW)==1,"recovered thrower must remain able to attack");
  }
  {
    MxTarget *t=scene(LK_BOSS,3);
    w.man.mode=MAN_AIM;t->stun=R(.005);t->wind=R(.5);t->b2=1;t->cd=RI(5);
    lord_ai(&w,t,R(.02));
    CHECK(t->stun<=0&&t->wind==0&&!t->b2,
          "a stun expiring this tick must still cancel the old windup");
  }
  {
    MxTarget *t=scene(LK_HEALER,1);t->stun=R(.5);t->heal=R(.01);t->mv=2;
    MxTarget *other=&w.targets[1];w.ntargets=2;
    other->alive=1;other->hp=1;other->hpmax=3;other->x=t->x+RI(3);other->y=t->y;
    lord_ai(&w,t,R(.02));
    CHECK(other->hp==1&&events(EV_HEAL)==0&&t->heal==R(.01)&&t->mv==0,
          "a stunned healer healed through the player's interrupt");
    t->stun=0;lord_ai(&w,t,R(.02));
    CHECK(other->hp==2&&events(EV_HEAL)==1,"healing must resume after recovery");
  }
  {
    MxTarget *t=scene(LK_WALKER,1);t->y=RI(59);w.man.y=t->y;
    t->stun=R(.1);t->wind=R(.01);t->cd=R(.2);
    lord_ai(&w,t,R(.02));
    CHECK(events(EV_HURT)==0&&t->wind==0,"stun must prevent the old swing");
    for(int frame=0;frame<90&&!events(EV_HURT);frame++)lord_ai(&w,t,R(.02));
    CHECK(events(EV_HURT)==1,"recovered walker must still attack normally");
    t=scene(LK_WALKER,1);t->y=RI(59);w.man.y=t->y;w.nofight=1;t->cd=0;
    for(int frame=0;frame<90;frame++)lord_ai(&w,t,R(.02));
    CHECK(events(EV_SWING)==0&&events(EV_HURT)==0,"fight-back-off behavior changed");
  }
  if(failures){fprintf(stderr,"COMBAT_RECOVERY_FAIL failures=%d checks=%d\n",failures,checks);return 1;}
  printf("COMBAT_RECOVERY_PASS checks=%d final_hp_immunity=1 counterpunch_interrupt=1 throw_interrupt=1 healer_interrupt=1 hard_bypass=1\n",checks);
  return 0;
}

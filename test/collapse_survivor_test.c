/* Rare collapsing-room escape: structural condition, HP weighting, one
 * chance only, no ghost invincibility, and all death causes. Fixed and float.
 * This compiles the real game core; no mocks for the escape rules. */
#include <stdio.h>
#include <string.h>
#include "../core/mx.c"
static MxWorld w;
#define CHECK(v,msg) do{if(!(v)){fprintf(stderr,"collapse-survivor: %s (line %d)\n",msg,__LINE__);return 1;}}while(0)
static void init(int hp){
  memset(&w,0,sizeof w);
  w.state=MX_PLAY;w.nrooms=1;
  MxRoom *r=&w.rooms[0];
  r->x=20;r->y=20;r->w=10;r->h=8;
  w.room_frame_orig[0]=36;
  w.room_frame_init[0]=0; /* wallpaper was already stripped by collapse */
  w.ntargets=1;
  MxTarget *t=&w.targets[0];
  t->x=R(24.5);t->y=RI(24);t->alive=1;
  t->hp=hp;t->hpmax=hp;t->room_id=0;
}
static unsigned successful_seed(int chance){
  const real threshold=rdiv(RI(chance),RI(4096));
  for(unsigned i=0;i<200000;i++){
    w.rng=i;
    if(rrand(&w)<threshold)return i;
  }
  return 0xffffffffu;
}
int main(void){
  init(3);
  MxTarget *t=&w.targets[0];
  /* Full room frame: all the roof, floor, and two posts still stand. */
  for(int x=20;x<30;x++){w.grid[IDX(x,19)]=MX_STONE;w.grid[IDX(x,28)]=MX_STONE;}
  for(int y=20;y<28;y++){w.grid[IDX(19,y)]=MX_STONE;w.grid[IDX(30,y)]=MX_STONE;}
  CHECK(paper_frame_support(&w,&w.rooms[0])==36,"room frame fixture wrong");
  CHECK(!collapse_survive(&w,t)&&!t->collapse_rolled,"intact room got miracle escape");
  /* More than 40% frame remains: a normal knockdown, not a collapse. */
  for(int x=20;x<30;x++)w.grid[IDX(x,19)]=0;
  for(int y=20;y<28;y++)w.grid[IDX(19,y)]=0;
  CHECK(paper_frame_support(&w,&w.rooms[0])==18,"partial frame fixture wrong");
  CHECK(!collapse_survive(&w,t)&&!t->collapse_rolled,"unbroken room qualified");
  memset(w.grid,0,sizeof w.grid);
  CHECK(paper_frame_support(&w,&w.rooms[0])==0,"full collapse fixture wrong");
  /* A collapse with no room to stand or duck is not survivable. */
  for(int y=21;y<=26;y++)for(int x=21;x<=27;x++)w.grid[IDX(x,y)]=MX_STONE;
  CHECK(!collapse_survive(&w,t)&&!t->collapse_rolled,"buried with no air pocket");
  memset(w.grid,0,sizeof w.grid);
  t->room_id=255;
  CHECK(!collapse_survive(&w,t),"outdoor target got room survivor event");
  t->room_id=0;
  /* Very rare even at 3HP, deterministic RNG seed for one known lucky try. */
  unsigned lucky=successful_seed(45+(3-1)*65);
  CHECK(lucky!=0xffffffffu,"failed to find one rare qualifying roll");
  w.rng=lucky;
  CHECK(collapse_survive(&w,t),"full collapse with lucky roll must survive");
  CHECK(t->hp==2&&t->alive&&t->collapse_rolled==2,"must lose HP and remain alive");
  CHECK(t->inv>0&&t->stun>0&&t->vy==0,"no survivor grace / injury");
  CHECK(w.nev==1&&w.ev[0].type==EV_SURVIVOR&&w.ev[0].m==2,"no survivor FX event");
  CHECK(!collapse_survive(&w,t),"one landlord survived multiple times");
  kill_hard(&w,t); CHECK(!t->alive,"escaped landlord permanently invulnerable");
  init(1); t=&w.targets[0];
  lucky=successful_seed(45);
  CHECK(lucky!=0xffffffffu,"missing one HP rare seed");
  w.rng=lucky;
  CHECK(collapse_survive(&w,t)&&t->hp==1,"one-HP miracle removed final playable target");
  init(1);t=&w.targets[0];
  /* A deliberately unlucky roll must be final, not rerolled next frame. */
  unsigned unlucky=0;
  for(;unlucky<200000;unlucky++){w.rng=unlucky;if(rrand(&w)>R(0.5))break;}
  CHECK(unlucky<200000,"could not choose unlucky seed");
  w.rng=unlucky;
  CHECK(!collapse_survive(&w,t)&&t->collapse_rolled==1,"unlucky attempt must mark one-time use");
  CHECK(!collapse_survive(&w,t),"unsuccessful collapse escape rerolled");
  puts("collapse survivors: intact vs failed frames, no-air pockets, HP injury, 1-HP miracles, one roll, expiring grace and killability PASSED");
  return 0;
}

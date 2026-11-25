/* Exercise the actual structural energy solver, not a duplicated formula. */
#include <stdio.h>
#include <string.h>
#include "../core/mx.c"
static MxWorld w;
static int bad;
#define CHECK(c,msg) do{if(!(c)){fprintf(stderr,"BREAKER FAIL: %s\n",msg);bad++;}}while(0)
static void scene(int power){
  init_tables();init_mats();memset(&w,0,sizeof w);
  w.state=MX_PLAY;w.man.mode=MAN_FLY;w.man.x=R(30.7);w.man.y=R(25.5);
  w.man.rad=MAN_R0;w.man.vx=RI(23);w.man.pwr=power;w.men=3;
}
static real wall(int power,int material,real health){
  scene(power);w.grid[IDX(31,25)]=material;w.hp[IDX(31,25)]=health;
  man_plow(&w);return w.grid[IDX(31,25)]?w.hp[IDX(31,25)]:0;
}
static real rubble(int power){
  scene(power);MxChunk *ch=&w.chunks[0];ch->active=1;ch->count=1;ch->x=R(31.5);ch->y=R(25.5);ch->rad=RI(1);ch->hitCd=RI(1);
  MxCell *a=&ch->cells[0];a->m=MX_STEEL;a->h=RI(150);a->wx=ch->x;a->wy=ch->y;a->edge=1;
  recompute(ch);man_plow(&w);return a->h;
}
int main(void){
  const real normal=wall(PW_NONE,MX_WOOD,RI(20)),breaker=wall(PW_BREAKER,MX_WOOD,RI(20));
  CHECK(normal>0&&breaker==0,"same launch breaches a wooden wall only with Breaker");
  real a=wall(PW_NONE,MX_STEEL,RI(150)),b=wall(PW_BREAKER,MX_STEEL,RI(150));
  CHECK(a<RI(150)&&b<a&&b>0,"Breaker does more partial damage without instant-kill steel");
  a=rubble(PW_NONE);b=rubble(PW_BREAKER);
  CHECK(a<RI(150)&&b<a&&b>0,"loose debris uses the same energy discount");
  mx_load(&w,0);CHECK(w.pw_left[PW_BREAKER]==1,"one per level");
  mx_select_power(&w,PW_BREAKER);CHECK(w.pw_sel==PW_BREAKER,"selection arms");
  mx_select_power(&w,PW_NONE);CHECK(w.pw_sel==PW_NONE&&w.pw_left[PW_BREAKER]==1,"normal disarms without spending");
  mx_select_power(&w,PW_BREAKER);
  mx_set_pull(&w,RI(-1),0);mx_release(&w);
  CHECK(w.man.mode==MAN_AIM&&w.pw_sel==PW_BREAKER&&w.pw_left[PW_BREAKER]==1,"short pull free");
  mx_set_pull(&w,RI(-8),RI(2));mx_release(&w);
  CHECK(w.man.mode==MAN_FLY&&w.man.pwr==PW_BREAKER&&w.pw_left[PW_BREAKER]==0,"valid shot spends once");
  mx_release(&w);CHECK(w.pw_left[PW_BREAKER]==0,"repeat release cannot spend again");
  mx_load(&w,0);mx_set_pull(&w,RI(-8),0);real nx,ny,tx,ty,gx,gy;
  mx_preview_point(&w,RI(1),&nx,&ny);mx_select_power(&w,PW_TURBO);
  mx_preview_point(&w,RI(1),&tx,&ty);
  CHECK(tx>nx,"turbo preview shows farther travel");
  mx_select_power(&w,PW_TOWEL);mx_preview_point(&w,RI(1),&gx,&gy);
  CHECK(gx==nx&&gy<ny,"towel preview shows lighter gravity");
  w.pw_left[PW_TOWEL]=0;mx_preview_point(&w,RI(1),&gx,&gy);
  CHECK(gx==nx&&gy==ny,"depleted preview returns to ordinary flight");
  if(bad)return 1;
  puts("BREAKER_PASS static=1 moving=1 stock=1 trajectory=1");return 0;
}

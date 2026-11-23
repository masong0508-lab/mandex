/* Moving TNT must respond to real collisions, not the act of detaching.
   Test private production code in both numeric modes, no mocked physics.
   This also locks in the per-cell blast hitbox and fuse persistence. */
#include <stdio.h>
#include <string.h>
#include "../core/mx.c"
static MxWorld W;
static int errors;
#define CHECK(c,msg) do{if(!(c)){fprintf(stderr,"FAIL mobile TNT: %s\n",msg);errors++;}}while(0)
static void scene(void){
  memset(&W,0,sizeof W);init_tables();init_mats();
  W.state=MX_PLAY;W.bh_lo=0;W.bh_hi=MX_ROWS+3;
  W.substeps=4;W.dz_lo=MX_ROWS*MX_COLS;W.dz_hi=-1;
}
static MxChunk *charge(real x,real y){
  MxChunk *ch=&W.chunks[0];ch->active=1;ch->count=1;
  ch->x=x;ch->y=y;ch->rad=RI(1);ch->cs=RI(1);
  MxCell *a=&ch->cells[0];a->wx=x;a->wy=y;a->h=RI(8);
  a->lx=a->ly=0;a->m=MX_TNT;a->edge=1;a->fuse=0;
  recompute(ch);ch->tnt_hit=0; /* tests injecting an impact target this TNT voxel */
  return ch;
}
static int events(int type){
  int total=0;for(int i=0;i<W.nev;i++)total+=(W.ev[i].type==type);
  return total;
}
static void wall(int x,int y,int mat){
  W.grid[IDX(x,y)]=(uint8_t)mat;
  W.hp[IDX(x,y)]=RI(mx_material_hp(mat));
}
int main(void){
  CHECK(sizeof(MxCell)==24,"fuse fits existing mobile voxel layout");
  CHECK(MX_NUM_LEVELS==60,"existing 60-level save/progression preserved");
  scene();
  MxChunk *ch=charge(R(20.5),R(20.5));
  CHECK(ch->has_tnt && !ch->tnt_fused,"detached TNT starts alive, unarmed");
  for(int i=0;i<90;i++)update_moving_tnt(&W);
  CHECK(!ch->cells[0].dead&&!ch->cells[0].fuse&&W.nexpl==0,
        "free movement and a soft drop do not explode TNT");
  scene();W.level=1;
  ch=charge(R(20.5),R(20.5));
  ch->tnt_impact=RI(29);update_moving_tnt(&W);
  CHECK(!ch->cells[0].fuse&&!ch->cells[0].dead&&events(EV_TNT_SAFE)==1,
        "a moderate blow may be a safe landing, with truthful feedback");
  W.time=R(0.4);ch->tnt_impact=RI(29);ch->tnt_hit=0;update_moving_tnt(&W);
  CHECK(ch->cells[0].fuse==TNT_FUSE_TICKS-1&&events(EV_TNT_FUSE)==1,
        "later moderate impact can ignite a reproducible short fuse");
  scene();ch=charge(R(30.5),R(30.5));
  ch->tnt_impact=RI(56);update_moving_tnt(&W);
  CHECK(ch->cells[0].fuse==TNT_FUSE_TICKS-1 && !ch->cells[0].dead && W.nexpl==0,
        "violent contact arms rather than teleporting the explosion to the original grid");
  CHECK(W.tnt_live==1 && W.tnt_soonest==TNT_FUSE_TICKS-1,
        "display status counts individual active charges and exact remaining fuse");
  for(int i=0;i<TNT_FUSE_TICKS-1;i++)update_moving_tnt(&W);
  CHECK(ch->cells[0].dead && W.nexpl==1 && W.expl[0][0]==30 && W.expl[0][1]==30,
        "fuse expires at the charge's current location, exactly once");
  CHECK(W.tnt_live==0 && W.tnt_soonest==0,
        "persistent warning disappears on exactly the frame the fuse finishes");
  W.ntargets=1;W.targets[0].alive=1;W.targets[0].x=R(33.5);W.targets[0].y=R(29.5);
  process_expl(&W);
  CHECK(events(EV_BOOM)==1 && !W.targets[0].alive,
        "actual delayed detonation destroys nearby enemies and plays the existing blast");
  process_expl(&W);
  CHECK(events(EV_BOOM)==1,"a consumed charge cannot explode twice");
  scene();ch=charge(R(20.8),R(20.8));wall(21,20,MX_RUBBER);
  ch->vx=RI(37);step_chunks(&W,0);
  CHECK(!ch->cells[0].dead&&ch->tnt_impact<RI(28),
        "soft rubber surface cushions the actual moving TNT hitbox");
  update_moving_tnt(&W);
  CHECK(!ch->cells[0].fuse,"rubber collision remains safe below the ignition threshold");
  scene();ch=charge(R(20.8),R(20.8));wall(21,20,MX_STEEL);
  ch->vx=RI(37);step_chunks(&W,0);
  CHECK(!ch->cells[0].dead&&ch->tnt_impact>=RI(28),
        "hitting a hard wall registers an actual nonfatal impact");
  update_moving_tnt(&W);
  CHECK(ch->cells[0].fuse>0 && events(EV_TNT_FUSE)==1,
        "hard-wall contact starts a real in-flight fuse");
  scene();ch=charge(R(20.8),R(20.8));wall(21,20,MX_STEEL);
  ch->vx=RI(76);step_chunks(&W,0);
  CHECK(ch->cells[0].dead&&W.nexpl==1,
        "a lethal impact detonates mobile TNT rather than silently deleting it");
  scene();ch=charge(R(80),R(30));
  ch->rad=RI(50);ch->cells[0].wx=R(39.5);ch->cells[0].wy=R(30.5);
  blast_moving_tnt(&W,R(39.5),R(30.5));
  CHECK(ch->cells[0].fuse==8,"blast checks transformed TNT location even with distant body centroid");
  scene();ch=charge(R(39.5),R(30.5));
  ch->rad=RI(65);ch->cells[0].wx=RI(95);ch->cells[0].wy=R(30.5);
  blast_moving_tnt(&W,R(39.5),R(30.5));
  CHECK(!ch->cells[0].fuse,"distant TNT does not ignite just because its parent chunk centre is nearby");
  scene();ch=charge(R(30.5),R(30.5));arm_moving_tnt(&W,ch,&ch->cells[0],8);
  settle(&W,ch,0);
  CHECK(!ch->active&&ch->cells[0].dead&&W.nexpl==1,
        "forced settling cannot silently swallow an already burning fuse");
  scene();ch=charge(R(30.5),R(30.5));
  destroy_moving_tnt(&W,&ch->cells[0]);
  destroy_moving_tnt(&W,&ch->cells[0]);
  CHECK(W.nexpl==1&&events(EV_CELL)==1,"direct hits queue one blast and one destruction effect");
  /* Off-axis impact on a long wall must not arm TNT far from the actual
     contact. A second fixture proves two TNT cells remain independent. */
  scene();ch=charge(R(20),R(20));ch->count=2;
  MxCell *remote=&ch->cells[1];memset(remote,0,sizeof *remote);
  remote->m=MX_STEEL;remote->h=RI(150);remote->lx=R(20.8);
  remote->ly=R(0.8);remote->edge=1;
  ch->rad=RI(25);recompute(ch);ch->tnt_hit=-1;wall(41,20,MX_STEEL);
  ch->vx=RI(37);step_chunks(&W,0);
  CHECK(ch->tnt_impact==0 && ch->tnt_hit==-1,
        "remote steel impact must not count as TNT contact");
  update_moving_tnt(&W);
  CHECK(!ch->cells[0].fuse,"hitting distant rubble cannot ignite TNT");
  scene();ch=charge(R(20),R(20));ch->count=2;
  remote=&ch->cells[1];memset(remote,0,sizeof *remote);
  remote->m=MX_TNT;remote->h=RI(8);remote->lx=R(20.8);
  remote->ly=R(0.8);remote->edge=1;
  ch->rad=RI(25);recompute(ch);ch->tnt_hit=-1;wall(41,20,MX_STEEL);
  ch->vx=RI(37);step_chunks(&W,0);
  CHECK(ch->tnt_hit==1 && ch->tnt_impact>=RI(28),
        "actual touched TNT cell recorded rather than first cap in chunk");
  update_moving_tnt(&W);
  CHECK(!ch->cells[0].fuse && ch->cells[1].fuse,
        "only directly struck TNT ignites in a two-charge falling chunk");
  /* A fuse already burning on one charge must not make another charge
     collision-proof. Nor may an armed voxel falsely say it survived. */
  scene();ch=charge(R(26.5),R(27.5));ch->count=2;
  remote=&ch->cells[1];memset(remote,0,sizeof *remote);
  remote->m=MX_TNT;remote->h=RI(8);
  remote->wx=R(28.5);remote->wy=R(27.5);
  remote->lx=RI(2);remote->edge=1;recompute(ch);
  arm_moving_tnt(&W,ch,&ch->cells[0],16);
  ch->tnt_hit=1;ch->tnt_impact=RI(56);
  update_moving_tnt(&W);
  CHECK(ch->cells[0].fuse==15 && remote->fuse==23 && W.tnt_live==2,
        "second charge arms on impact while the first charge's fuse burns");
  scene();ch=charge(R(30.5),R(30.5));W.level=1;
  arm_moving_tnt(&W,ch,&ch->cells[0],16);
  ch->tnt_hit=0;ch->tnt_impact=RI(29);
  update_moving_tnt(&W);
  CHECK(ch->cells[0].fuse==15 && events(EV_TNT_SAFE)==0,
        "another collision cannot label an already armed charge as safe");
  scene();ch=charge(R(26.5),R(27.5));ch->count=2;
  remote=&ch->cells[1];memset(remote,0,sizeof *remote);
  remote->m=MX_TNT;remote->h=RI(8);remote->wx=R(28.5);remote->wy=R(27.5);
  remote->lx=RI(2);remote->edge=1;recompute(ch);
  arm_moving_tnt(&W,ch,&ch->cells[0],16);
  arm_moving_tnt(&W,ch,remote,8);
  update_moving_tnt(&W);
  CHECK(W.tnt_live==2 && W.tnt_soonest==7,
        "two TNT charges in one chunk produce an accurate count and urgent timer");
  for(int i=0;i<7;i++)update_moving_tnt(&W);
  CHECK(W.tnt_live==1 && W.tnt_soonest==8,
        "first detonation leaves the second independently burning charge visible");
  if(errors){fprintf(stderr,"MOBILE_TNT_FAIL failures=%d\n",errors);return 1;}
  puts("MOBILE_TNT_PASS safe_move=1 impact=1 fuse=1 chain=1 precise_hitbox=1 fixed_size=24");
  return 0;
}

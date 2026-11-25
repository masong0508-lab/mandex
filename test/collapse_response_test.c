/* Native structural response regression: float and DS 20.12 fixed builds.
   Uses the production support traversal, wobble snapper and body contacts. */
#include <stdio.h>
#include <string.h>
#include "../core/mx.c"
static MxWorld W;
static int bad;
#define CHECK(c,msg) do{if(!(c)){fprintf(stderr,"COLLAPSE FAIL: %s (line %d)\n",msg,__LINE__);bad++;}}while(0)
static void scene(void){
  memset(&W,0,sizeof W);
  init_tables();init_mats();
  W.state=MX_PLAY;W.wob_enabled=1;
  W.bh_lo=0;W.bh_hi=MX_ROWS+3;W.substeps=3;
  W.dz_lo=MX_ROWS*MX_COLS;W.dz_hi=-1;
}
static void put(int x,int y,int m){
  W.grid[IDX(x,y)]=(uint8_t)m;W.hp[IDX(x,y)]=RI(MATS[m].hp);
}
static int active(void){
  int n=0;for(int k=0;k<MX_MAX_CHUNKS;k++)n+=W.chunks[k].active;return n;
}
static int fallen(void){
  int n=0;for(int k=0;k<MX_MAX_CHUNKS;k++)if(W.chunks[k].active)n+=W.chunks[k].count;
  return n;
}
static MxChunk *body(int n,real x,real y,int rest){
  MxChunk *c=&W.chunks[n];c->active=1;c->count=1;
  c->x=x;c->y=y;c->rad=RI(1);c->cs=RI(1);c->rest=(uint8_t)rest;
  MxCell *a=&c->cells[0];a->m=MX_STEEL;a->h=RI(150);
  a->wx=x;a->wy=y;a->lx=a->ly=0;a->edge=1;
  recompute(c);return c;
}
int main(void){
  CHECK(MX_CHUNK_CELLS==512,"the DS rigid-body memory ceiling changed");
  scene();
  for(int y=10;y<30;y++)for(int x=45;x<75;x++)put(x,y,MX_WOOD);
  support_pass(&W);
  CHECK(active()==1&&W.chunks[0].count==512,
        "first oversized slab remained hovering instead of falling");
  CHECK(W.need_support,"large collapse did not request a continuation pass");
  int n=0,edges=0,cut=0;
  for(int i=0;i<MX_COLS*MX_ROWS;i++)n+=W.grid[i]!=0;
  static const int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};
  for(int i=0;i<W.chunks[0].count;i++){
    const MxCell *a=&W.chunks[0].cells[i];
    edges+=a->edge!=0;
    int x=rfloor(a->wx),y=rfloor(a->wy);
    for(int d=0;d<4;d++){
      int nx=x+dx[d],ny=y+dy[d];
      if(INB(nx,ny)&&W.grid[IDX(nx,ny)]){
        cut++;CHECK(a->edge,"new cut face is missing from collision detection");
      }
    }
  }
  CHECK(n==88&&cut>0&&edges>0&&edges<512,
        "split face lost cells or made every interior voxel collidable");
  for(int k=0;k<8&&W.need_support;k++)support_pass(&W);
  CHECK(fallen()==600&&active()>=2&&!W.need_support,
        "large disconnected structure did not fully detach within bounded passes");
  n=0;for(int i=0;i<MX_COLS*MX_ROWS;i++)n+=W.grid[i]!=0;
  CHECK(n==0,"unsupported cells linger on the world grid after detachment");
  scene();for(int y=57;y<60;y++)put(40,y,MX_WOOD);support_pass(&W);
  CHECK(active()==0&&W.grid[IDX(40,59)]==MX_WOOD,
        "grounded structure fell without being disconnected");
  scene();put(30,30,MX_WOOD);put(31,30,MX_WOOD);
  int i=IDX(30,30),j=IDX(31,30);
  real full=joint_break_limit(&W,i,j,MX_WOOD,MX_WOOD);
  W.hp[j]=RI(10);
  real half=joint_break_limit(&W,i,j,MX_WOOD,MX_WOOD);
  W.hp[j]=RI(5);
  real quarter=joint_break_limit(&W,i,j,MX_WOOD,MX_WOOD);
  CHECK(full>half&&half>quarter&&quarter>0,
        "injured walls retain unchanged joint fracture strain");
  W.hp[j]=RI(10);W.ox[j]=rmul(full,R(0.9));
  wob_nact=1;wob_act[0]=(uint16_t)i;wob_nb[0]=1;
  wob_cx[0]=30;wob_cy[0]=30;
  wobble_check(&W);
  CHECK((W.brk[i]&1)&&(W.brk[j]&2),
        "moderate strain failed to snap an already weakened joint");
  scene();
  MxChunk *a=body(0,R(20.3),R(20.5),0);
  MxChunk *b=body(1,R(20.8),R(20.5),1);
  a->vx=RI(8);step_chunks(&W,0);
  CHECK(!b->rest,"substantive chunk impact failed to wake settled debris");
  scene();a=body(0,R(20.3),R(20.5),0);
  b=body(1,R(20.8),R(20.5),1);
  a->vx=RI(2);step_chunks(&W,0);
  CHECK(b->rest,"gentle pile contact woke a sleeper and may cause jitter");
  /* A full rubble pool must not chew through the entire disconnected
     region or leave it hanging after a slot becomes available. */
  scene();
  for(int k=0;k<MX_MAX_CHUNKS;k++)body(k,RI(8+k*2),RI(15),0);
  for(int y=27;y<30;y++)for(int x=45;x<50;x++)put(x,y,MX_WOOD);
  support_pass(&W);
  CHECK(W.retry_cd==20&&W.grid[IDX(45,27)]==MX_WOOD&&active()==MX_MAX_CHUNKS,
        "full pool lost unsupported cells or failed to schedule retry");
  W.chunks[0].cells[0].dead=1;
  W.man.mode=MAN_AIM;W.men=1;
  mx_update(&W,R(1.0/60.0));
  CHECK(W.retry_cd==0&&!W.grid[IDX(45,27)]&&W.chunks[0].active&&W.chunks[0].count==15,
        "freed rubble slot did not trigger the next eligible support pass");
  /* Exact maximum-distance radius using the original per-voxel sqrt. */
  real rad=0;
  for(int k=0;k<W.chunks[0].count;k++){
    MxCell *cell=&W.chunks[0].cells[k];
    real rr=rsqrt(rmul(cell->lx,cell->lx)+rmul(cell->ly,cell->ly));
    if(rr>rad)rad=rr;
  }
  CHECK(W.chunks[0].rad==rad+RI(1),"single final sqrt changed the slab bound");
  if(bad){fprintf(stderr,"COLLAPSE_RESPONSE_FAIL failures=%d\n",bad);return 1;}
  puts("COLLAPSE_RESPONSE_PASS first_512=1 remainder=1 progressive_joints=1 hard_wake=1 gentle_rest=1");
  return 0;
}

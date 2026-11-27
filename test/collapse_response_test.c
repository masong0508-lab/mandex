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
/* Independent pre-optimization BFS, including the native bounded ring. */
static uint8_t reference_hb[MX_COLS*MX_ROWS];
static uint16_t reference_queue[8192];
static void reference_support(void){
  const int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};
  unsigned head=0,tail=0;
  memset(reference_hb,255,sizeof reference_hb);
  for(int x=0;x<MX_COLS;x++){
    int i=IDX(x,MX_ROWS-1);
    if(W.grid[i]){reference_hb[i]=0;reference_queue[tail++&8191]=(uint16_t)i;}
  }
  while(head!=tail){
    int i=reference_queue[head++&8191],x=i%MX_COLS,y=i/MX_COLS,h=reference_hb[i];
    for(int d=0;d<4;d++){
      int nx=x+dx[d],ny=y+dy[d];
      if(!INB(nx,ny)||((W.brk[i]>>d)&1))continue;
      int j=IDX(nx,ny),m=W.grid[j],nh=dy[d]==-1?0:h+1;
      if(m&&nh<reference_hb[j]&&nh<=MATS[m].lim+(W.anch[j]?8:0)&&tail-head<8191){
        reference_hb[j]=(uint8_t)nh;reference_queue[tail++&8191]=(uint16_t)j;
      }
    }
  }
}
static uint32_t rng=0x4e97b2e;
static unsigned random_byte(void){rng=rng*1664525u+1013904223u;return rng>>24;}
static void support_parity(void){
  const int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};
  for(int y=0;y<MX_ROWS;y++)for(int x=0;x<MX_COLS;x++)for(int d=0;d<4;d++)
    CHECK(!!(support_border(x,y)&(1u<<d))==!INB(x+dx[d],y+dy[d]),"border mask must prevent row wrap and out-of-bounds");
  for(int fixture=0;fixture<15;fixture++){
    scene();
    for(int y=0;y<MX_ROWS;y++)for(int x=0;x<MX_COLS;x++){
      int i=IDX(x,y);
      if(random_byte()<(unsigned)(fixture%5*60+15)){
        put(x,y,random_byte()%2?MX_WOOD:MX_STEEL);
        W.anch[i]=(uint8_t)(random_byte()<64);
      }
      if(random_byte()<(unsigned)(fixture/5*90))W.brk[i]=(uint8_t)(random_byte()&15u);
    }
    reference_support();support_pass(&W);
    CHECK(!memcmp(reference_hb,W.hb,sizeof reference_hb),"linear traversal changed ground reach or broken-joint propagation");
  }
}
int main(void){
  support_parity();
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
  if(bad){fprintf(stderr,"COLLAPSE_RESPONSE_FAIL failures=%d\n",bad);return 1;}
  puts("COLLAPSE_RESPONSE_PASS first_512=1 remainder=1 progressive_joints=1 hard_wake=1 gentle_rest=1");
  return 0;
}

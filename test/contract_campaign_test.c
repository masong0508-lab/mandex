/* Native campaign regression: actual shared builder, float/fixed builds.
   Raw-grid FNV fingerprints were obtained from the production web builder.
   Then load each playable scene and advance its real physics. */
#include <stdio.h>
#include <string.h>
#include "../core/mx.c"
static MxWorld W;
static int fail;
#define CHECK(c,l,msg) do{if(!(c)){fprintf(stderr,"CONTRACT %d FAIL: %s\n",(l)+1,msg);fail++;}}while(0)
static uint32_t fingerprint(void){
  uint32_t h=2166136261u;
  for(int k=0;k<MX_ROWS*MX_COLS;k++)h=(h^W.grid[k])*16777619u;
  return h;
}
int main(void){
  static const uint32_t expected[16]={1978458262u,3881162397u,2234426049u,2234742033u,3701005221u,2662884443u,2340284964u,2409039517u,3635458059u,508532293u,489688324u,984490101u,3067476221u,989929033u,2740109274u,1882119693u};
  static const int targets[16]={7,6,6,7,6,8,7,5,8,6,8,7,8,9,9,11},rooms[16]={7,5,6,7,4,9,7,5,10,6,8,7,8,9,13,9},cells[16]={325,412,348,364,594,402,325,332,421,300,531,314,369,582,621,754};
  CHECK(MX_CONTRACT_START==60&&MX_NUM_LEVELS==76,60,"old IDs preserved and all contracts available");
  init_tables();init_mats();
  for(int i=0;i<MX_CONTRACT_COUNT;i++){
    int l=MX_CONTRACT_START+i;
    memset(&W,0,sizeof W);W.rng=0x2545F491u;
    build_contract(&W,i);
    CHECK(fingerprint()==expected[i],l,"raw native and browser voxel maps differ");
    CHECK(mx_cells_left(&W)==cells[i]&&W.ntargets==targets[i]&&W.nrooms==rooms[i],l,"wrong structure or encounter count");
    CHECK(W.nrooms<=MX_MAX_ROOMS&&W.ntargets<=MX_MAX_TARGETS&&W.ntubes<=MX_MAX_TUBES&&W.npads<=MX_MAX_PADS,l,"pool budget exceeded");
    CHECK(W.walk==(i==6||i==11),l,"walking contract flag mismatch");
    for(int k=0;k<W.ntargets;k++){
      const MxTarget *t=&W.targets[k];
      int x=rfloor(t->x),y=rfloor(t->y+R(0.875));
      int clear=y>=MX_ROWS-1||W.grid[IDX(x,y)]!=0;
      for(int yy=y-5;yy<y;yy++)for(int xx=x-2;xx<=x+2;xx++)
        if(!INB(xx,yy)||W.grid[IDX(xx,yy)])clear=0;
      CHECK(clear,l,"landlord spawned inside solid masonry or without a floor");
    }
    mx_load(&W,l);mx_set_fight(&W,0);
    CHECK(W.level==l&&W.men>=6&&W.ntargets==targets[i],l,"real level loading failed");
    CHECK(W.pw_left[CONTRACT_POWER[i]]==2,l,"recommended ability bonus missing");
    for(int p=1;p<PW_COUNT;p++)
      CHECK(W.pw_left[p]==(p==CONTRACT_POWER[i]?2:1),l,"other inventory changed");
    CHECK(mx_level_name(l)&&strlen(mx_level_tactic(l))<=50,l,"missing name or readable tactic");
    for(int k=0;k<60;k++)mx_update(&W,R(1.0/60.0));
    CHECK(W.state==MX_PLAY&&mx_targets_alive(&W)>0,l,"contract completed itself while idle");
    printf("CONTRACT %02d voxels=%d rooms=%d targets=%d hash=%08x\n",i+1,cells[i],rooms[i],targets[i],expected[i]);
  }
  if(fail)return 1;
  puts("CONTRACT_CAMPAIGN_PASS 16 raw-map parity, clear spawns, bounded pools, power stock and idle physics");
  return 0;
}

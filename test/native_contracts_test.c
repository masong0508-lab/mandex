/* Include the production C unit so the pre-support map can be compared
 * byte-for-byte via deterministic browser-generated FNV-1a fingerprints.
 * Also load all levels through the public core in both numeric modes. */
#include <stdio.h>
#include <string.h>
#include "../core/mx.c"
static MxWorld W;
static const uint32_t expected[MX_CONTRACT_COUNT]={
 0x871ebd91u,
 0xf6851ffcu,
 0xad3660afu,
 0xe9aa692cu,
 0xf551f164u,
 0x1e5791b9u,
 0x84acd7e0u,
 0x35915d74u,
 0xba855522u,
 0x1882c57fu,
 0xf176a468u,
 0x64591a2du,
 0xcc12c867u,
 0xbd9d6eaeu,
 0xa46ecac6u,
 0x247cb718u,
 0xaf3f211fu,
 0xb9fae952u,
 0xd28d8bd9u,
 0x86cb7135u,
 0x2bc4dd81u,
 0x2e989e94u,
 0x18dfcb4bu,
 0x24eea42fu,
 0x0f2b09afu,
 0x1aa08632u,
 0x44f553f9u,
 0x55cb695cu,
 0x349f6942u,
 0xe7c80b2fu,
 0x516763e0u,
 0x792b4347u,
 0x7b30dfc4u,
 0xe7d33d79u,
 0x2a7a5d3eu,
 0xc017ec51u,
 0x5c59a637u,
 0x9a2fd0b6u,
 0xf78791d0u,
 0xa0f96bb0u,
 0x42b167efu,
 0x12a5025au,
 0xefe450e6u,
 0x6d771248u,
 0x17316475u,
 0xdde578bcu,
 0xd45227afu,
 0xce76eef6u,
 0xf2aff73au,
 0x0ce5eac3u,
 0x3c62679cu,
 0x9767f22bu,
 0xf702e472u,
 0x42be12f7u,
 0xeacef4f0u,
 0xf491b0fdu,
 0xaaa0ea5du,
 0xadf5d168u,
 0x505f03e0u,
 0x9aee9fd3u,
 0x4fe85042u,
 0x2f8e843fu,
 0x143df6ccu,
 0x11072f0fu,
 0x7b2abfe2u,
 0x9d67be03u,
 0x970ea112u,
 0xa03ac10eu,
 0x95d3e7d4u,
 0x86fb349du,
 0xc42d4ac6u,
 0xf2734ef1u
};
static uint32_t feed(uint32_t h,unsigned x){return (h^x)*16777619u;}
int main(void){
  init_tables();init_mats();
  for(int i=0;i<MX_CONTRACT_COUNT;i++){
    memset(&W,0,sizeof W);W.rng=0x2545F491u;build_contract(&W,i);
    uint32_t h=2166136261u;
    for(int k=0;k<MX_COLS*MX_ROWS;k++)h=feed(h,W.grid[k]);
    h=feed(h,CONTRACT_MEN[i%4]);h=feed(h,W.ntargets);
    for(int k=0;k<W.ntargets;k++){
      h=feed(h,rfloor(W.targets[k].x));h=feed(h,rfloor(W.targets[k].y)+1);
      h=feed(h,W.targets[k].kind);
    }
    if(h!=expected[i]){fprintf(stderr,"contract %d grid/target fingerprint: %08x != %08x\n",i,(unsigned)h,(unsigned)expected[i]);return 1;}
    mx_load(&W,MX_NUM_LEVELS+i);
    if(W.level!=MX_NUM_LEVELS+i||W.ntargets<2||W.ntargets>12||W.men!=CONTRACT_MEN[i%4]||W.state!=MX_PLAY||W.nrooms<1)return 2;
    if(!mx_level_name(W.level)[0]||strlen(mx_level_tactic(W.level))>50)return 3;
    for(int frame=0;frame<8;frame++)mx_update(&W,R(1.0/60));
    if(W.state!=MX_PLAY||mx_targets_alive(&W)<1)return 4;
  }
  if(mx_level_section_step(40,1)!=60||mx_level_section_step(60,1)!=64||
     mx_level_section_step(60,-1)!=40||mx_level_section_step(0,-1)!=MX_TOTAL_LEVELS-4||
     mx_level_section_step(MX_TOTAL_LEVELS-1,1)!=0)return 5;
  printf("NATIVE_CONTRACTS_PASS maps=%d parity=%d navigation=1\n",MX_CONTRACT_COUNT,MX_CONTRACT_COUNT);return 0;
}

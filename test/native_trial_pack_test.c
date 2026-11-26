/* Validate the actual native trial decoder, not a reimplementation. */
#include <stdio.h>
#include <string.h>
#include "../core/mx.c"
#include "mx_meta.h"
#include "mx_save_journal.h"
static const uint32_t expected[MX_TRIAL_COUNT]={
  0xbb8469u,0xed0920d1u,0xe28b7965u,0xedb99e41u,0x239edfc1u,0x1ddf8c39u,0xaf571f55u,0xd48dfa09u,
  0xca535e2du,0xc91c2c35u,0x49646b01u,0xcdc44f9du,0x95f4f2cdu,0x37ce74b9u,0x573103cdu,0x649c55d9u,
  0xe87a1051u,0xb54d2355u,0x2ab7af35u,0xde409055u,0x2cfc2d99u,0x34593fd1u,0x1daa5231u,0xa34a0b99u,
  0xb1f068f1u,0xd3faab5du,0x473785fdu,0x84b4e975u,0xfdc413f9u,0x8861479u,0xe33959e1u,0x3405ec4du,
  0x7b9215c1u,0x2f8e9fc5u,0xf44053e1u,0x3ddbaa17u,0x5044e48fu,0x1f0e75ffu,0xe1a95177u,0x413d36fu,
  0x4afa0e51u,0x98d821f5u,0x5b150adu,0xd526d20du,0x71cc0e81u,0xa31d8015u,0xc260bd6du,0xd634c7c5u,
  0x725a79ddu,0x1bb9358du,0x2e097dddu,0x226dd7e5u,0x79671a9du,0x72d90cedu,0xb481ead9u,0x30bb0025u,
  0x92fa4301u,0xfbe30265u,0xfe35fab5u,0xa52c8585u,0x34352939u,0xa7f33269u,0x193d9c55u,0x4f11dd81u,
  0xd8c0b61u,0x91aacd6du,0xc8c30ea9u,0x5b8eb545u,0x7a8b6551u,0x66160e75u,0x4ff4adf1u,0xcc1bac05u,
  0x38d26195u,0xf9c6e009u,0x8f5892c5u,0x31253fd1u,0xff5f0d9du,0x25aa12f9u,0x6a782ca1u,0x30deb011u,
  0x2ae96c8du,0x88250d65u,0x410b635du,0x289715ddu,0x7edbad0du,0xfba549edu,0xeae50845u,0x6de53d85u,
  0xf58bedddu,0x32da05cdu,0xf4a55eadu,0xd7a374adu,0xc2a2863du,0xb8e51035u,0x435a94du,0x341d2601u,
  0x63565a9u,0x5c6c34d5u,0x525e5b85u,0x291215u,0x92ccbefdu,0x8fda5f2du,0x664fb1f1u,0x2be0b7f5u,
  0x51d2acfdu,0xcb064a01u,0x1de06fedu,0xeacef6b9u,0xe7d6c5b1u,0x41eda09u,0x14618855u,0xd0aa6b2du,
  0x94755589u,0x6da43fc9u,0x6440a579u,0x1305d45u,0x17fbe5c5u,0xdbbb5845u,0x30c82325u,0xe913d0c5u,
  0xe27c2f61u,0xa038d259u,0x537fa845u,0xf18c5981u,0xe22f2dadu,0xaab5b529u,0xad982cdu,0x9d768349u,
  0x4a94051du,0x3892ae5u,0x8cf5b2adu,0xba52cd6du,0x20772a79u,0x44d0c115u,0xff48bca9u,0x4b286a05u,
  0x750494a5u,0xcb254f05u,0x8125a85u,0x95c4ad85u,0xc6d536adu,0x1049b38du,0x98ade351u,0x6c69ef55u,
  0x257870f1u,0x9817a489u,0x6466e3d5u,0xa3f1e89u,0x611814f5u,0x12db05e9u,0x4a44c2f1u,0x74390c2du,
  0x7061d5b1u,0x44ead239u,0x7904545u,0xb8cde155u,0x5eb0e1c5u,0xaa191385u,0x7bc31595u,0x6e5e0315u
};
static MxWorld W;
static uint32_t digest(void){
  uint32_t h=2166136261u;
  for(int i=0;i<MX_ROWS*MX_COLS;i++)h=(h^W.grid[i])*16777619u;
  return h;
}
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL trial %d at line %d: %s\n",i,__LINE__,#c);return 1;}}while(0)
int main(void){
  init_tables();init_mats();
  for(int i=0;i<MX_TRIAL_COUNT;i++){
    const MxTrialLevel *t=&MX_TRIALS[i];
    memset(&W,0,sizeof W);build_trial(&W,i);
    CHECK(digest()==expected[i]);
    CHECK(W.ntargets==t->target_n&&W.nrooms==t->room_n&&W.npads==t->pad_n&&W.ntubes==t->tube_n&&W.walk==t->walk);
    mx_load(&W,MX_TRIAL_START+i);
    CHECK(W.level==MX_TRIAL_START+i&&W.men==t->men&&W.ntargets==t->target_n);
    CHECK(digest()==expected[i]); /* no spawn collapse or misplaced supports */
    CHECK(strcmp(mx_level_name(W.level),t->name)==0&&strcmp(mx_level_tactic(W.level),t->tip)==0);
    CHECK(mx_targets_alive(&W)==t->target_n);
    mx_set_fight(&W,0);
    for(int frame=0;frame<30;frame++)mx_update(&W,R(1.0/60.0));
    CHECK(mx_targets_alive(&W)==t->target_n);
  }
  int i=0; MxStats S,T;mx_meta_init(&S);
  CHECK(mx_level_unlocked(&S,MX_TRIAL_START)&&mx_level_unlocked(&S,MX_NUM_LEVELS-1));
  CHECK(mx_trial_district_step(0,1)==MX_TRIAL_START);
  CHECK(mx_trial_district_step(MX_TRIAL_START,1)==MX_TRIAL_START+20);
  CHECK(mx_trial_district_step(MX_NUM_LEVELS-1,1)==0);
  CHECK(MX_JOURNAL_DISK_BYTES==1536&&MX_SAVE_MAX==1176);
  S.stars[59]=2;S.best[59]=12345;S.stars[219]=3;S.best[219]=67890;
  uint8_t disk[MX_JOURNAL_DISK_BYTES]={0};int n=mx_journal_pack(&S,0,NULL,7,disk,sizeof disk);
  CHECK(n==MX_JOURNAL_MAX&&mx_journal_record_size(disk,sizeof disk)==n);
  uint32_t gen=0;int outfit=0;mx_meta_init(&T);
  CHECK(mx_journal_unpack(&T,&outfit,NULL,&gen,disk,n)&&gen==7&&T.best[59]==12345&&T.best[219]==67890);
  printf("NATIVE_TRIAL_PACK_PASS levels=%d raw=%d command_bytes=%zu\n",MX_TRIAL_COUNT,MX_TRIAL_COUNT*MX_ROWS*MX_COLS,sizeof MX_TRIAL_DATA);
  return 0;
}

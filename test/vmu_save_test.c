/* VMU storage regression: execute the REAL dual-slot adapter using a
   512-byte VMU sector model.  Verifies interleaved generations, torn
   writes, slot corruption and no-card behavior without physical hardware. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "mx_meta.h"
static MxStats stats;
static int outfit_sel, nofight;
typedef int file_t;
#define FILEHND_INVALID (-1)
#define O_RDONLY 1
#define O_WRONLY 2
static unsigned char disk[2][1024], exists[2];
static int lengths[2];
static int active=-1, mode, available=1, short_write;
static int slot(const char *name){
  size_t n=strlen(name);
  return n && name[n-1]=='A' ? 0 : n && name[n-1]=='B' ? 1 : -1;
}
static file_t fs_open(const char *name, int flags){
  int s=slot(name);
  if(!available || s<0 || (flags==O_RDONLY && !exists[s]))return FILEHND_INVALID;
  active=s;mode=flags;if(flags==O_WRONLY)exists[s]=1;
  return s;
}
static int fs_total(file_t f){return exists[f]?lengths[f]:0;}
static int fs_read(file_t f,void *buf,int count){
  if(!exists[f])return -1;
  if(count>lengths[f])count=lengths[f];
  memcpy(buf,disk[f],(size_t)count);return count;
}
static int fs_write(file_t f,const void *buf,int count){
  if(short_write){memcpy(disk[f],buf,25);lengths[f]=25;return 25;}
  if(count>1024)count=1024;
  memcpy(disk[f],buf,(size_t)count);lengths[f]=count;return count;
}
static int fs_close(file_t f){(void)f;active=-1;return 0;}
static int fs_unlink(const char *name){
  int s=slot(name);if(s<0 || !available)return -1;
  exists[s]=0;return 0;
}
#define __sh__ 1
#include "../dreamcast/vmu_save.h"
#undef __sh__

static int fail;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL VMU test line %d: %s\n",__LINE__,#c);fail++;}}while(0)
int main(void){
  mx_meta_init(&stats);
  stats.wins=10;stats.best[0]=100;outfit_sel=11;nofight=1;
  /* Simulate the previous release's one-block journal with an authentic
     60-level payload and both CRCs; the first upgraded load must recover it. */
  { unsigned char payload[MX_SAVE_MAX],*p=disk[1]+8;
    MxOptions opt;mx_options_default(&opt);opt.nofight=1;
    mx_meta_pack_opt(&stats,11,&opt,payload,sizeof payload);
    memcpy(disk[1],"MXJ1",4);mx_journal_put32(disk[1]+4,0);
    memcpy(p,payload,66);p[5]=MX_NUM_LEVELS;
    memcpy(p+66,payload+66,MX_NUM_LEVELS);
    memcpy(p+66+MX_NUM_LEVELS,payload+66+MX_TOTAL_LEVELS,5);
    memcpy(p+71+MX_NUM_LEVELS,payload+71+MX_TOTAL_LEVELS,4*MX_NUM_LEVELS);
    int off=71+5*MX_NUM_LEVELS;p[off]=16;
    mx_journal_put32(p+off+1,mx_crc32(p,off+1));
    mx_journal_put32(p+off+5,mx_crc32(disk[1],8+off+5));
    lengths[1]=512;exists[1]=1;mx_meta_init(&stats);dc_save_load();
    CHECK(dc_save_slot==1&&stats.wins==10&&stats.best[0]==100&&stats.best[MX_NUM_LEVELS]==0&&nofight==1);
  }
  stats.best[MX_TOTAL_LEVELS-1]=12345;stats.stars[MX_TOTAL_LEVELS-1]=3;
  dc_save_sync();
  CHECK(dc_save_slot==0 && exists[0] && dc_save_generation==1 && lengths[0]==1024);
  CHECK(dc_save_status==2);
  stats.wins=20;stats.best[0]=200;
  dc_save_sync();
  CHECK(dc_save_slot==1 && exists[1] && dc_save_generation==2);
  /* Reboot: choose newest intact generation. */
  mx_meta_init(&stats);outfit_sel=nofight=0;dc_save_slot=-1;dc_save_generation=0;
  dc_save_load();
  CHECK(dc_save_slot==1 && stats.wins==20 && stats.best[0]==200);
  CHECK(stats.best[MX_TOTAL_LEVELS-1]==12345&&stats.stars[MX_TOTAL_LEVELS-1]==3);
  CHECK(outfit_sel==11 && nofight==1);
  /* Both saves identical: no needless VMU flash wear. */
  dc_save_sync();CHECK(dc_save_generation==2);
  /* Newest slot corrupt: recover previous slot without losing progress. */
  disk[1][20]^=0x80;
  mx_meta_init(&stats);outfit_sel=nofight=0;dc_save_slot=-1;dc_save_generation=0;
  dc_save_load();
  CHECK(dc_save_slot==0 && stats.wins==10 && stats.best[0]==100);
  stats.wins=11;
  short_write=1;dc_save_sync();short_write=0; /* simulates pulled VMU */
  CHECK(dc_save_status==-1 && dc_save_slot==0);
  mx_meta_init(&stats);dc_save_slot=-1;dc_save_generation=0;dc_save_load();
  CHECK(dc_save_slot==0 && stats.wins==10);
  /* Card missing: saving fails safely, progress in RAM stays intact. */
  available=0;stats.wins=33;dc_save_sync();
  CHECK(dc_save_status==-1 && stats.wins==33);
  printf("vmu_save_test: %s\n",fail?"FAILED":"all passed");
  return fail!=0;
}

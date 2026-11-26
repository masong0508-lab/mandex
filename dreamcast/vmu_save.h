/* Dreamcast: recoverable VMU progress in slot A1.
   Embedded once after the front-end's stats/outfit/nofight declarations.
   Two independent 1024-byte files alternate (legacy 512-byte slots are read) after wins and menu changes,
   so an interrupted VMU write cannot overwrite the last valid slot.
   No per-frame write or heap allocation.  No VMU = no save, never no game. */
#ifndef MX_DC_VMU_SAVE_H
#define MX_DC_VMU_SAVE_H
#include "mx_save_journal.h"

#if defined(__sh__) && !defined(MX_DC_NO_VMU)
static const char *const dc_save_paths[2] = {
  "/vmu/a1/MANDEX_A", "/vmu/a1/MANDEX_B"
};
#define MX_DC_SLOT_BYTES 1024
#if MX_JOURNAL_MAX > MX_DC_SLOT_BYTES
#error "VMU journal exceeds two blocks"
#endif
static uint32_t dc_save_generation;
static int dc_save_slot = -1, dc_save_status;
static uint8_t dc_saved_payload[MX_SAVE_MAX];
static int dc_saved_len;

static void dc_save_load(void){
  uint8_t record[MX_DC_SLOT_BYTES];
  /* Track the newest VALID record separately from the generation high-water
     mark. An interrupted replacement of an older, corrupt slot must never
     reuse its generation: a short write could otherwise accidentally repair
     the old checksum and resurrect that stale slot on the next reboot. */
  uint32_t valid_generation=0, seen_generation=0;
  int any_seen=0;
  dc_save_slot=-1; dc_save_generation=0;
  for(int i = 0; i < 2; i++){
    file_t fd=fs_open(dc_save_paths[i],O_RDONLY);
    if(fd == FILEHND_INVALID)continue;
    int sz=fs_total(fd);
    int got=sz>=14&&sz<=MX_DC_SLOT_BYTES?fs_read(fd,record,sz):0;
    fs_close(fd);
    if(got!=sz||got<14)continue;
    if(record[0]=='M' && record[1]=='X' && record[2]=='J' && record[3]=='1'){
      uint32_t g=mx_journal_get32(record+4);
      if(!any_seen || mx_journal_newer(g,seen_generation)){
        seen_generation=g; any_seen=1;
      }
    }
    MxStats trial; mx_meta_init(&trial);
    MxOptions opt; int outf=0; uint32_t generation=0;
    /* Determine the original payload length before checking its outer CRC;
       old one-block saves contain fewer levels and zero padding. */
    int ver=record[12],nl=record[13];
    if(ver<1||ver>MX_SAVE_VERSION)continue;
    int rn=12+75+5*nl+(ver>=2?1:0);
    if(rn>got||rn>MX_JOURNAL_MAX)continue;
    if(!mx_journal_unpack(&trial,&outf,&opt,&generation,record,rn))
      continue;
    if(dc_save_slot>=0 && !mx_journal_newer(generation,valid_generation))
      continue;
    valid_generation=generation;dc_save_slot=i;dc_save_status=1;
    stats=trial;outfit_sel=outf;nofight=opt.nofight;
  }
  if(dc_save_slot>=0){
    dc_save_generation=any_seen ? seen_generation : valid_generation;
    MxOptions opt; mx_options_default(&opt);opt.nofight=(uint8_t)nofight;
    dc_saved_len=mx_meta_pack_opt(&stats,outfit_sel,&opt,
                                 dc_saved_payload,sizeof dc_saved_payload);
  }
}
static void dc_save_sync(void){
  uint8_t payload[MX_SAVE_MAX],disk[MX_DC_SLOT_BYTES];
  MxOptions opt;mx_options_default(&opt);opt.nofight=(uint8_t)nofight;
  int n=mx_meta_pack_opt(&stats,outfit_sel,&opt,payload,sizeof payload);
  if(!n || (dc_saved_len==n && !memcmp(payload,dc_saved_payload,n)))return;
  memset(disk,0,sizeof disk);
  int rn=mx_journal_pack(&stats,outfit_sel,&opt,dc_save_generation+1,
                         disk,MX_JOURNAL_MAX);
  if(!rn)return;
  int slot=dc_save_slot==0 ? 1 : 0;
  /* Only erase the target slot. If this fails, the previous is still good. */
  fs_unlink(dc_save_paths[slot]);
  file_t fd=fs_open(dc_save_paths[slot],O_WRONLY);
  if(fd==FILEHND_INVALID){dc_save_status=-1;return;}
  int written=fs_write(fd,disk,sizeof disk);
  int closed=fs_close(fd);
  if(written!=(int)sizeof disk || closed<0){dc_save_status=-1;return;}
  dc_save_slot=slot;dc_save_generation++;dc_save_status=2;
  memcpy(dc_saved_payload,payload,(size_t)n);dc_saved_len=n;
}
#else
static void dc_save_load(void){}
static void dc_save_sync(void){}
#endif
#endif

#ifndef MX_SAVE_JOURNAL_H
#define MX_SAVE_JOURNAL_H
/*
 * MANDEX power-loss-tolerant save envelope (3DS first; reusable by VMU
 * adapters).  The existing MXS1/MXS2 payload and its CRC stay untouched.
 * Each of two independent storage slots contains:
 *   0..3  "MXJ1"
 *   4..7  monotonically increasing u32 generation, little endian
 *   8..   mx_meta_pack_opt() payload (with its own CRC32)
 *   last4 CRC32 over the header AND payload
 * A damaged or truncated slot is rejected; the other may be recovered.
 * No dynamic allocation, changed MxStats layout, or platform I/O here.
 */
#include "mx_meta.h"

#define MX_JOURNAL_MAX (MX_SAVE_MAX + 12)
#define MX_JOURNAL_DISK_BYTES (((MX_JOURNAL_MAX + 511) / 512) * 512)

static inline void mx_journal_put32(uint8_t *p, uint32_t v){
  p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8);
  p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
static inline uint32_t mx_journal_get32(const uint8_t *p){
  return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
         ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static inline int mx_journal_pack(const MxStats *stats, int outfit,
                                  const MxOptions *opt, uint32_t generation,
                                  uint8_t *dst, int cap){
  if(!dst || cap < MX_JOURNAL_MAX) return 0;
  dst[0]='M'; dst[1]='X'; dst[2]='J'; dst[3]='1';
  mx_journal_put32(dst+4, generation);
  int n=mx_meta_pack_opt(stats,outfit,opt,dst+8,cap-12);
  if(!n) return 0;
  mx_journal_put32(dst+8+n,mx_crc32(dst,8+n));
  return n+12;
}
/* VMU records are padded to whole blocks. Read the actual historical
 * payload size from its validated version/count fields; old 60-level
 * journals remain readable after the trial expansion. */
static inline int mx_journal_record_size(const uint8_t *src,int available){
  if(!src || available<14 || src[0]!='M'||src[1]!='X'||src[2]!='J'||src[3]!='1' ||
     src[8]!='M'||src[9]!='X'||src[10]!='S'||src[11]!='1')return 0;
  const int ver=src[12],nl=src[13];
  if(ver<1||ver>MX_SAVE_VERSION||nl<1||nl>MX_NUM_LEVELS)return 0;
  const int n=12+66+nl+4+1+4*nl+(ver>=2?1:0)+(ver>=3?1:0)+4;
  return n<=available&&n<=MX_JOURNAL_MAX?n:0;
}
static inline int mx_journal_unpack(MxStats *stats, int *outfit, MxOptions *opt,
                                    uint32_t *generation, const uint8_t *src,
                                    int len){
  if(!stats || !outfit || !generation || !src ||
     len<12 || len>MX_JOURNAL_MAX ||
     src[0]!='M' || src[1]!='X' || src[2]!='J' || src[3]!='1') return 0;
  if(mx_crc32(src,len-4)!=mx_journal_get32(src+len-4)) return 0;
  if(!mx_meta_unpack_opt(stats,outfit,opt,src+8,len-12)) return 0;
  *generation=mx_journal_get32(src+4);
  return 1;
}
/* True for newer generation even after a u32 counter wrap.
   Call only after BOTH slots passed integrity checks. */
static inline int mx_journal_newer(uint32_t a,uint32_t b){
  return (int32_t)(a-b)>0;
}
#endif

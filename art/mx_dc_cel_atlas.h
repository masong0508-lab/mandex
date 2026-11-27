/* Alternate Dreamcast character data; reuse the existing two PVR atlases. */
#ifndef MX_DC_CEL_ATLAS_H
#define MX_DC_CEL_ATLAS_H
#include <stddef.h>
#include "manspr.h"
#ifndef MX_DCART
#define MX_DCART
#endif
#include "dcart.h"
#if defined(__has_include)
# if __has_include("dcart_cel.h")
#  include "dcart_cel.h"
#  define MX_CEL_DC_AVAILABLE 1
# endif
#endif
#ifndef MX_CEL_DC_AVAILABLE
#define MX_CEL_DC_AVAILABLE 0
#endif
static inline const short *mx_dc_frame(int frame,int cel){
#if MX_CEL_DC_AVAILABLE
  if(cel)return MC_DCM_FR[frame];
#else
  (void)cel;
#endif
  return DCM_FR[frame];
}
static inline const short *mx_dc_hand(int frame,int cel){
#if MX_CEL_DC_AVAILABLE
  if(cel)return MC_DC_HAND[frame];
#else
  (void)cel;
#endif
  return MS_HAND[frame];
}
static inline const unsigned short *mx_dc_rle(int wolf,int cel,int *len){
#if MX_CEL_DC_AVAILABLE
  if(cel){
    *len=wolf?MC_DCT_WMAN_RLE_LEN:MC_DCT_MAN_RLE_LEN;
    return wolf?MC_DCT_WMAN_RLE:MC_DCT_MAN_RLE;
  }
#else
  (void)cel;
#endif
  *len=wolf?DCT_WMAN_RLE_LEN:DCT_MAN_RLE_LEN;
  return wolf?DCT_WMAN_RLE:DCT_MAN_RLE;
}
static inline int mx_dc_rle_valid(const unsigned short *src,int len,size_t total){
  if(len<=0||(len&1))return 0;
  size_t n=0;
  for(int i=0;i<len;i+=2){
    size_t count=src[i];
    if(!count||count>total-n)return 0;
    n+=count;
  }
  return n==total;
}
static inline void mx_dc_rle_decode(const unsigned short *src,int len,unsigned short *dst){
  for(int i=0;i<len;i+=2){
    unsigned count=src[i];
    while(count--)*dst++=src[i+1];
  }
}
#endif

/* Runtime selector shared by the native indexed renderers. */
#ifndef MX_CEL_ATLAS_H
#define MX_CEL_ATLAS_H
#include "manspr.h"
#if defined(__has_include)
# if __has_include("manspr_cel.h")
#  include "manspr_cel.h"
#  define MX_CEL_INDEXED_AVAILABLE 1
# endif
#endif
#ifndef MX_CEL_INDEXED_AVAILABLE
# define MX_CEL_INDEXED_AVAILABLE 0
#endif
static inline const MsFrame *mx_man_frame(int frame,int cel){
#if MX_CEL_INDEXED_AVAILABLE
  if(cel)return &MC_FRAMES[frame];
#else
  (void)cel;
#endif
  return &MS_FRAMES[frame];
}
static inline const short *mx_man_hand(int frame,int cel){
#if MX_CEL_INDEXED_AVAILABLE
  if(cel)return MC_HAND[frame];
#else
  (void)cel;
#endif
  return MS_HAND[frame];
}
#endif

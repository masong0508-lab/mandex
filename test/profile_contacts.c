#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mx.h"
/* Measures solver work per frame on L5, edge culling vs not (build with -DMX_NO_EDGE). */
static MxWorld w;
int main(void){
  uint64_t tot=0; int frames=0, peak=0;
  for(int L=0; L<5; L++){
    memset(&w,0,sizeof w); mx_load(&w,L);
    mx_set_pull(&w, R(-9), R(5)); mx_release(&w);
    for(int f=0; f<420; f++){
      uint32_t before=w.stat_cells; mx_update(&w, R(1.0/60.0));
      int d=(int)(w.stat_cells-before); tot+=d; if(d>peak) peak=d; frames++;
    }
  }
  printf("chunk-voxel contact checks: total=%llu avg/frame=%.1f peak/frame=%d\n",(unsigned long long)tot,(double)tot/frames,peak);
  return 0;
}

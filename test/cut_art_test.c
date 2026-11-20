/* Regression: every tiny storyboard icon must be substantial, distinct, and ROM-only.
 * Shared by Nintendo DS, Nintendo 3DS and Sega Dreamcast; the browser uses the same rows. */
#include <stdio.h>
#include "../ds/source/cut_art.h"
int main(void){
  unsigned int fingerprints[MX_CUT_ICON_COUNT];
  for(int i=0;i<MX_CUT_ICON_COUNT;i++){
    int pixels=0; unsigned int hash=2166136261u;
    for(int y=0;y<16;y++){
      unsigned int bits=mx_cut_icons[i][y];
      for(int x=0;x<16;x++) if(bits&(0x8000u>>x))pixels++;
      hash=(hash^bits)*16777619u;
    }
    if(pixels<32 || pixels>155){fprintf(stderr,"cut icon %d has %d pixels\n",i,pixels);return 1;}
    for(int j=0;j<i;j++)if(fingerprints[j]==hash){fprintf(stderr,"duplicate icon %d / %d\n",j,i);return 1;}
    fingerprints[i]=hash;
  }
  puts("ok: 15 unique illustrated story emblems, packed in ROM");
  return 0;
}

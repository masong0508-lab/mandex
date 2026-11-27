/* Real shared native animation table, used by DS / 3DS / Dreamcast.
 * Check the entire motion timeline, not just a few frame numbers. */
#include <stdio.h>
#include <string.h>
#define MS_ENUM_ONLY
#include "../art/manspr.h"
#include "../art/manidle.h"
#define CHECK(v) do{if(!(v)){fprintf(stderr,"native cel motion line %d: %s\n",__LINE__,#v);return 1;}}while(0)
int main(void){
  const int ids[]={GAG_WIGGLE,GAG_FART};
  unsigned ticks=0;
  for(unsigned g=0;g<sizeof ids/sizeof ids[0];g++){
    const MiStep *seq=MI_GAGS[ids[g]];
    int n=0;
    while(n<MI_MAXSTEPS&&seq[n].ticks)n++;
    CHECK(n<MI_MAXSTEPS&&n>=27);
    for(int i=0;i<=12;i++)CHECK(seq[i].frame==MSF_SPIN_0+i);
    int at=n-13;
    for(int i=0;i<11;i++)CHECK(seq[at+i].frame==MSF_SPIN_R11-i);
    CHECK(seq[n-2].frame==MSF_SPIN_0&&seq[n-1].frame==MSF_IDLE_A);
    MiState m;memset(&m,0,sizeof m);mi_reset(&m);m.gag=ids[g];
    for(int i=0;i<n;i++){
      for(int t=0;t<seq[i].ticks;t++){
        CHECK(m.gag==ids[g]&&m.step==i);
        CHECK(mi_frame(&m,0)==seq[i].frame);
        mi_update(&m);ticks++;
      }
    }
    CHECK(!mi_active(&m));
  }
  printf("NATIVE_CEL_MOTION_PASS full_turns=4 ticks=%u\n",ticks);
  return 0;
}

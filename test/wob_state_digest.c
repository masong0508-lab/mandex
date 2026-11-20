/* Exact whole-world deterministic state digest. Compare the same numeric
 * mode with and without a proposed wobble optimization. No sampled pixels,
 * no tolerance: even one changed spring/AI/collision byte changes the hash. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "mx.h"
static MxWorld w;
int main(void){
  uint64_t digest=1469598103934665603ull;
  for(int level=0;level<MX_NUM_LEVELS;level++){
    memset(&w,0,sizeof w);
    mx_load(&w,level);
    mx_set_pull(&w,R(-9),R(5));mx_release(&w);
    for(int frame=0;frame<360;frame++){
      if(w.man.mode==MAN_FLY&&frame==90)mx_dive(&w);
      if(w.man.mode==MAN_WALK)mx_walk_input(&w,1,0,(frame&15)==0,0);
      mx_update(&w,R(1.0/60.0));
      if(frame%30==0||frame==359){
        const unsigned char *p=(const unsigned char*)&w;
        for(size_t i=0;i<sizeof w;i++){
          digest^=p[i];digest*=1099511628211ull;
        }
      }
    }
  }
  printf("full_world_digest=%016llx levels=%d frames_per_level=360\n",
        (unsigned long long)digest,MX_NUM_LEVELS);
  return 0;
}

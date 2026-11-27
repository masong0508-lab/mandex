/* Every material pair, every damage-class boundary, and the fracture
 * threshold on both sides. The oracle is the original production limit. */
#include <stdio.h>
#include "../core/mx.c"
static MxWorld world;
int main(void){
  init_mats();
  unsigned checks=0,small=0;
#ifdef MX_FIXED
  const real eps=1;
#else
  const real eps=R(0.00001);
#endif
  for(int m=1;m<MX_NMAT;m++)for(int j=1;j<MX_NMAT;j++){
    real mh=RI(MATS[m].hp),jh=RI(MATS[j].hp);
    const real a[]={0,mh/4-eps,mh/4,mh/4+eps,mh/2-eps,mh/2,mh/2+eps,mh};
    const real b[]={0,jh/4-eps,jh/4,jh/4+eps,jh/2-eps,jh/2,jh/2+eps,jh};
    world.grid[0]=(uint8_t)m;world.grid[1]=(uint8_t)j;
    for(unsigned u=0;u<sizeof a/sizeof *a;u++)for(unsigned v=0;v<sizeof b/sizeof *b;v++){
      world.hp[0]=a[u];world.hp[1]=b[v];
      real lim=joint_break_limit(&world,0,1,m,j);
      const real probes[]={0,R(0.04),R(0.08),R(0.084),R(0.1),R(0.2),
                           lim-eps,lim,lim+eps,R(1),R(3),R(6),R(100)};
      for(unsigned n=0;n<sizeof probes/sizeof *probes;n++){
        real strain=probes[n];
        int expect=strain>lim;
        int actual=joint_should_break(&world,0,1,m,j,strain);
        checks++;
#if MX_WOB_JOINT_EARLY_REJECT
        if(strain<=wob_min_joint)small++;
#endif
        if(actual!=expect){
          fprintf(stderr,"joint mismatch materials=%d/%d hp=%g/%g strain=%g limit=%g\n",m,j,
                  (double)RTOF(a[u]),(double)RTOF(b[v]),(double)RTOF(strain),(double)RTOF(lim));
          return 1;
        }
      }
    }
  }
  printf("WOB_JOINT_THRESHOLD_PASS checks=%u conservative_early_rejections=%u\n",checks,small);
  return 0;
}

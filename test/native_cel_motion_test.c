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
  /* The same RNG seed chooses the same ordinary gag in both modes.
     A selected burp alone becomes the Golden Cels feast performance;
     retaining GAG_BURP as last prevents immediate repeats. */
  int feast_choices=0;
  for(unsigned seed=1;seed<=512;seed++){
    MiState classic={0},cel={0};
    mi_reset(&classic);
    classic.last=-1;classic.rng=seed;classic.wait=1;
    cel=classic;
    mi_update(&classic);
    mi_update_style(&cel,1);
    CHECK(classic.rng==cel.rng&&classic.last==cel.last);
    if(classic.gag==GAG_BURP){
      CHECK(cel.gag==GAG_FEAST&&cel.last==GAG_BURP);
      feast_choices++;
    }else CHECK(cel.gag==classic.gag&&cel.fx==classic.fx);
  }
  CHECK(feast_choices>0);
  const MiStep expected[]={
    {MSF_IDLE_A,11,FX_NONE},{MSF_IDLE_B,21,FX_NONE},
    {MSF_BELLY_A,25,FX_SLAP},{MSF_BELLY_B,8,FX_NONE},
    {MSF_BURP_A,19,FX_GRUNT},{MSF_BURP_B,36,FX_BURP},
    {MSF_FLEX_A,15,FX_NONE},{MSF_POSE_A,24,FX_SPARKLE},
    {MSF_IDLE_A,21,FX_NONE}
  };
  MiState feast={0};mi_reset(&feast);feast.gag=GAG_FEAST;
  feast.last=GAG_BURP;feast.fx=MI_GAGS[GAG_FEAST][0].fx;
  unsigned feast_ticks=0;
  for(unsigned i=0;i<sizeof expected/sizeof expected[0];i++){
    for(int t=0;t<expected[i].ticks;t++){
      CHECK(feast.gag==GAG_FEAST&&feast.step==(int)i);
      CHECK(mi_frame(&feast,0)==expected[i].frame);
      CHECK(feast.fx==(t==0?expected[i].fx:FX_NONE));
      /* A style change never splices a half-played animation. */
      mi_update_style(&feast,feast_ticks<80);feast_ticks++;
    }
  }
  CHECK(feast_ticks==180&&!mi_active(&feast)&&feast.last==GAG_BURP);
  mi_reset(&feast);feast.wait=1;mi_update_style(&feast,1);
  CHECK(feast.gag!=GAG_FEAST&&feast.gag!=GAG_BURP);
  mi_reset(&feast);CHECK(!mi_active(&feast)&&feast.fx==FX_NONE);
  printf("NATIVE_FEAST_PASS choices=%d ticks=%u\n",feast_choices,feast_ticks);
  printf("NATIVE_CEL_MOTION_PASS full_turns=4 ticks=%u\n",ticks);
  return 0;
}

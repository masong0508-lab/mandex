/* Built against the actual generated alternate headers in ROM CI. */
#include <stdio.h>
#include <string.h>
#include "mx_cel_atlas.h"
#include "mx_dc_cel_atlas.h"
#define CHECK(v) do{if(!(v)){fprintf(stderr,"atlas line %d: %s\n",__LINE__,#v);return 1;}}while(0)
int main(void){
  CHECK(MX_CEL_INDEXED_AVAILABLE && MX_CEL_DC_AVAILABLE);
  int changed=0,changed_wolf=0,hd_changed=0;
  for(int i=0;i<MSF_COUNT;i++){
    const MsFrame *a=mx_man_frame(i,0),*b=mx_man_frame(i,1);
    CHECK(a==&MS_FRAMES[i]&&b!=a&&mx_man_frame(i,0)==a);
    CHECK(b->w>0&&b->h>0&&b->px);
    for(int k=0;k<b->w*b->h;k++)CHECK(b->px[k]<=MS_NPAL);
    int differs=a->w!=b->w||a->h!=b->h||a->ax!=b->ax||a->ay!=b->ay||
      memcmp(a->px,b->px,(size_t)a->w*a->h)!=0;
    if(i<MSF_NBASE)changed+=differs;else changed_wolf+=differs;
    CHECK(mx_man_hand(i,0)==MS_HAND[i]);
    const short *h=mx_dc_frame(i,1);
    CHECK(h!=DCM_FR[i]&&mx_dc_frame(i,0)==DCM_FR[i]);
    CHECK(h[0]>=0&&h[1]>=0&&h[2]>0&&h[3]>0&&h[0]+h[2]<=DCT_MAN_W&&h[1]+h[3]<=DCT_MAN_H);
    hd_changed+=memcmp(h,DCM_FR[i],13*sizeof(short))!=0;
    CHECK(mx_dc_hand(i,0)==MS_HAND[i]);
    CHECK(!memcmp(mx_man_hand(i,1),mx_dc_hand(i,1),3*sizeof(short)));
  }
  CHECK(changed>0&&changed_wolf>0&&hd_changed>0);
  for(int wolf=0;wolf<2;wolf++)for(int cel=0;cel<2;cel++){
    int len=0;const unsigned short *p=mx_dc_rle(wolf,cel,&len);
    CHECK(mx_dc_rle_valid(p,len,(size_t)DCT_MAN_W*DCT_MAN_H));
    CHECK(p==mx_dc_rle(wolf,cel,&len));
  }
  unsigned short bad[]={0,1}; CHECK(!mx_dc_rle_valid(bad,2,1));
  bad[0]=2; CHECK(!mx_dc_rle_valid(bad,2,1));
  CHECK(!mx_dc_rle_valid(bad,1,2));
  printf("LIVE_NATIVE_ATLAS_PASS human=%d wolf=%d hd_geometry=%d\n",changed,changed_wolf,hd_changed);
  return 0;
}

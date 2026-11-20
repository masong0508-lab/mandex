/* Shared-console room backdrop destruction: float and fixed builds.
 * Test real room/frame layouts, exposed-wall impacts, partial structural
 * damage, full frame loss, all 60 spawn layouts, and district transitions.
 * Includes mx.c to inspect the same static helpers used by the game. */
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "../core/mx.c"
static MxWorld w;
static uint8_t untouched[MX_ROWS*MX_COLS];
#define CHECK(c,msg) do{if(!(c)){fprintf(stderr,"FAIL room backdrop: %s (line %d)\n",msg,__LINE__);return 1;}}while(0)
static int select_room(void){
  int best=-1,area=0;
  for(int r=0;r<w.nrooms;r++){
    const MxRoom *p=&w.rooms[r];
    if(p->w<10||p->h<6||w.room_frame_init[r]<10)continue;
    int a=p->w*p->h;
    if(a>area){area=a;best=r;}
  }
  return best;
}
static int paper_cell(const MxRoom *r){
  for(int y=r->y+2;y<r->y+r->h-1;y++)
    for(int x=r->x+2;x<r->x+r->w-1;x++)
      if(INB(x,y)&&!w.grid[IDX(x,y)]&&w.paper[IDX(x,y)])
        return IDX(x,y);
  return -1;
}
static void drop_cell(int x,int y,int *quota){
  if(*quota>0&&INB(x,y)&&w.grid[IDX(x,y)]){
    w.grid[IDX(x,y)]=0; --*quota;
  }
}
static void remove_frame(const MxRoom *r,int count){
  int q=count;
  for(int x=r->x;x<r->x+r->w;x++){
    drop_cell(x,r->y-1,&q);drop_cell(x,r->y+r->h,&q);
  }
  for(int y=r->y;y<r->y+r->h;y++){
    drop_cell(r->x-1,y,&q);drop_cell(r->x+r->w,y,&q);
  }
}
int main(void){
  /* A completely intact world must not shed wallpaper on spawn. */
  for(int l=0;l<MX_NUM_LEVELS;l++){
    mx_load(&w,l);memcpy(untouched,w.paper,sizeof untouched);
    w.paper_dirty=1;paper_room_update(&w);
    CHECK(memcmp(untouched,w.paper,sizeof untouched)==0,"intact level lost wallpaper");
  }
  mx_load(&w,39);
  int k=select_room();
  CHECK(k>=0,"no large backed room in Landlord HQ");
  MxRoom r=w.rooms[k];
  int first=w.room_frame_init[k],q=paper_cell(&r);
  CHECK(q>=0,"no exposed interior wallpaper in chosen room");
  int x=q%MX_COLS,y=q/MX_COLS,shade=w.paper[q];
  CHECK(damage_cell(&w,x,y,RI(3))==0&&w.paper[q]==shade,
        "weak hit tore exposed backdrop");
  CHECK(damage_cell(&w,x,y,RI(8))==0&&w.paper[q]==0,
        "strong impact failed to tear exposed backdrop");
  mx_load(&w,39);k=select_room();r=w.rooms[k];q=paper_cell(&r);
  CHECK(q>=0,"flight room missing");
  w.man.mode=MAN_FLY;
  w.man.x=RI(q%MX_COLS)+R(0.5);w.man.y=RI(q/MX_COLS)+R(0.5);
  w.man.vx=0;w.man.vy=0;
  man_step(&w,0);
  CHECK(w.paper[q]==0,"flying man failed to tear wallpaper");
  mx_load(&w,39);k=select_room();r=w.rooms[k];q=paper_cell(&r);
  CHECK(q>=0,"partial-damage room missing");
  first=w.room_frame_init[k];
  remove_frame(&r,first/4);
  w.paper_dirty=1;paper_room_update(&w);
  CHECK(w.paper[q]!=0&&w.room_frame_init[k]==first,
        "minor frame damage destroyed whole room");
  remove_frame(&r,INT_MAX);
  w.paper_dirty=1;paper_room_update(&w);
  CHECK(w.room_frame_init[k]==0,"unsupported room did not collapse");
  for(int yy=r.y;yy<r.y+r.h;yy++)for(int xx=r.x;xx<r.x+r.w;xx++)
    if(INB(xx,yy))CHECK(!w.paper[IDX(xx,yy)],"floating backdrop survived frame loss");
  /* Changing Landlord HQ districts rebuilds the baseline instead of
     carrying dead scenery or stale support values into the next map. */
  hq_next_district(&w);
  CHECK(w.district==1&&w.nrooms>0&&!w.paper_dirty,
        "district change did not reset room support");
  int backed=0;
  for(int n=0;n<w.nrooms;n++)backed+=w.room_frame_init[n]>0;
  CHECK(backed>0,"new district failed to seed supports");
  printf("room_backdrop_test: all 60 levels, impacts, collapse and HQ district passed\n");
  return 0;
}

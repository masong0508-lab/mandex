/* Chapter 3 adds a deterministic destructible landmark to each Wolfman
   street. Test actual loaded worlds in fixed (DS) and float (3DS/DC) modes.
   Web parity: PARITY=1 sh test/run_tests.sh (requires Playwright). */
#include <stdio.h>
#include <string.h>
#include "mx.h"
static MxWorld W;
static int failed;
static void ok(int cond,int level,const char *msg){
  if(!cond){fprintf(stderr,"FAIL L%d: %s\n",level+1,msg);failed++;}
}
#define CELL(x,y) W.grid[(y)*MX_COLS+(x)]
int main(void){
  ok(MX_TRIAL_START == 60 && MX_NUM_LEVELS == 220 && MX_CH3_START==40,40,"save-compatible chapter indices");
  for(int i=0;i<20;i++){
    int a0=48+(i%3)*4, aw=8+(i%4)*2;
    int b0=80+(i%2)*4, gap=b0-(a0+aw);
    int ax=a0+aw+(gap-8)/2, tier=i/5, site=i%4;
    int floors[3]={2+(i%4),2+((i+1)%4),2+(i%3)};
    int count=0, nh=i>=8?3:2;
    for(int k=0;k<nh;k++)for(int f=0;f<floors[k];f++)
      if((f==floors[k]-1&&k==0)||f==0||(f+k+i)%2==0)count++;
    if(count<4)count++;
    mx_load(&W,40+i);
    ok(gap>=12 && ax>=a0+aw+2 && ax+8<=b0-2,40+i,"annex does not overlap original buildings");
    ok(W.ntargets==count+1 && W.ntargets<=MX_MAX_TARGETS,40+i,"one discoverable extra landlord, within pool");
    ok(W.nrooms<=MX_MAX_ROOMS,40+i,"room metadata fits bounded pool");
    if(W.ntargets>0){
      const MxTarget *t=&W.targets[W.ntargets-1];
      int kind=site==0?LK_THROWER:site==1?(tier?LK_SPRINTER:LK_WALKER):
               site==2?LK_WALKER:(tier?LK_SHIELD:LK_BAT);
      ok(rfloor(t->x)==ax+4 && t->kind==kind && t->alive,40+i,
         "annex defender matches the scene and progression");
    }
    ok(CELL(ax+4,MX_ROWS-1)==(site==1?MX_RUBBER:site==2?MX_SAND:MX_WOOD),40+i,
       "ground floor correctly uses wood or a soft impact bed");
    if(site==0){
      ok(CELL(ax,53)==MX_GLASS && CELL(ax+2,57)==MX_TNT,40+i,"glass booth and ground charge");
    }else if(site==1){
      ok(CELL(ax,53)==MX_RUBBER && CELL(ax+2,49)==MX_TNT,40+i,"rubber rebound and upper charge");
      ok(CELL(ax+2,51)==MX_RUBBER && CELL(ax+5,MX_ROWS-1)==MX_RUBBER,40+i,
         "upper and lower rubber shelves cushion moving TNT");
    }else if(site==2){
      ok(CELL(ax,53)==MX_GLASS && CELL(ax+1,45)==MX_SAND &&
         CELL(ax+3,49)==MX_TNT,40+i,"sand hopper and release pin");
      ok(CELL(ax+2,MX_ROWS-1)==MX_SAND && CELL(ax+5,MX_ROWS-1)==MX_SAND,40+i,
         "soft sand floor forms a second crash zone");
    }else{
      ok(CELL(ax,53)==MX_BRICK && CELL(ax+4,41)==MX_STONE &&
         CELL(ax+2,57)==MX_TNT,40+i,"armored roof and accessible charge");
    }
    /* In the final 12 maps, a second four-cell landmark appears in the
       second alley. Inspect actual loaded cells and keep room/NPC pools
       beneath their fixed maximums. */
    if(i>=8){
      int x2=80+(i%2)*4,w2=10+(i%5)*2;
      int gap2=108-(x2+w2), bx=x2+w2+(gap2-4)/2;
      ok(gap2>=6 && bx>=x2+w2+1 && bx+4<=107,40+i,
         "second alley fits the main houses without overlap");
      int wall=site==0||site==2?MX_GLASS:site==1?MX_RUBBER:MX_STEEL;
      int roof=site==0||site==3?MX_GLASS:MX_WOOD;
      ok(CELL(bx,56)==wall && CELL(bx+1,51)==roof,40+i,
         "second alley has the correct themed wall and roof");
      int charge_row=site==2?54:57;
      ok(CELL(bx+1,charge_row)==MX_TNT &&
         CELL(bx+2,charge_row+1)==MX_TNT,40+i,
         "detonatable alley charge is two by two and inside the booth");
      if(site==0)
        ok(CELL(bx+1,48)==MX_GLASS && CELL(bx+2,49)==MX_GLASS,
           40+i,"glass signal remains visible above its roof");
      if(site==1)
        ok(CELL(bx+1,59)==MX_RUBBER && CELL(bx+2,49)==MX_RUBBER,
           40+i,"spring gate has soft landing floor and rubber cap");
      if(site==2)
        ok(CELL(bx+1,56)==MX_SAND && CELL(bx+2,58)==MX_SAND &&
           CELL(bx+1,48)==MX_SAND,40+i,
           "sandbag spill includes real lower and upper sand blocks");
      if(site==3)
        ok(CELL(bx+1,48)==MX_STEEL && CELL(bx+2,49)==MX_STEEL,
           40+i,"steel signal anchored above the breakable roof");
    }
    ok(strlen(mx_level_tactic(40+i))<=52,40+i,
       "console mission tactic fits the small handheld level banner");
    int cells=mx_cells_left(&W);
    for(int f=0;f<180;f++)mx_update(&W,R(1.0/60.0));
    int chunks=0;
    for(int k=0;k<MX_MAX_CHUNKS;k++)chunks+=W.chunks[k].active;
    ok(mx_cells_left(&W)==cells && chunks==0,40+i,"landmark stays standing at load");
  }
  if(failed){fprintf(stderr,"%d Wolfman annex failures\n",failed);return 1;}
  puts("OK: 20 Wolfman street encounters, stable and bounded in this numeric mode");
  return 0;
}

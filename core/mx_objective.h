/*
 * MANDEX: objective clarity shared by DS, 3DS and Dreamcast.
 *
 * Destroyed cells are optional score; evicting landlords is the win condition.
 * The game can remain in MX_PLAY for 1.6 seconds after the last eviction,
 * and Landlord HQ advances to another district rather than ending the level.
 * This read-only projection keeps all HUDs truthful without extra gameplay
 * fields, allocations, timers, or changes to physics/save formats.
 */
#ifndef MX_OBJECTIVE_H
#define MX_OBJECTIVE_H
#include "mx.h"

enum {
  MX_OBJ_EVICT, MX_OBJ_LAST, MX_OBJ_SECURE, MX_OBJ_DISTRICT,
  MX_OBJ_COMPLETE, MX_OBJ_FAILED, MX_OBJ_NO_MEN, MX_OBJ_SUMMIT
};
typedef struct {
  int phase, remaining, total;
  int direction; /* -1: target to left, +1: target to right, 0: aligned/not needed */
  int target_index; /* final living landlord, or -1; only a read-only snapshot */
} MxObjective;

enum { MX_EDGE_NONE, MX_EDGE_LEFT, MX_EDGE_RIGHT, MX_EDGE_UP, MX_EDGE_DOWN };
typedef struct { int dir, x, y; } MxObjectiveEdge;

/* Small reusable screen-space navigation hint. The viewport rectangle
   excludes the top mission HUD and bottom action controls. A cue is drawn
   ONLY when the last landlord is outside that playable rectangle. */
static inline MxObjectiveEdge mx_objective_edge(int sx,int sy,int width,
                                               int height,int top,int bottom){
  MxObjectiveEdge e={MX_EDGE_NONE,0,0};
  if(width<48||height<48||top+bottom+28>=height)return e;
  int floor=height-bottom;
  if(sx>=0 && sx<width && sy>=top && sy<floor)return e;
  if(sx<0)e.dir=MX_EDGE_LEFT;
  else if(sx>=width)e.dir=MX_EDGE_RIGHT;
  else if(sy<top)e.dir=MX_EDGE_UP;
  else e.dir=MX_EDGE_DOWN;
  e.x=sx<22?22:sx>width-22?width-22:sx;
  e.y=sy<top+9?top+9:sy>floor-10?floor-10:sy;
  return e;
}

static inline MxObjective mx_objective_snapshot(const MxWorld *w){
  MxObjective o = {MX_OBJ_EVICT, 0, w->ntargets, 0, -1};
  const MxTarget *last = 0;
  for(int i=0;i<w->ntargets;i++){
    if(w->targets[i].alive){ o.remaining++; last=&w->targets[i]; o.target_index=i; }
  }
  if(w->state==MX_WIN){ o.phase=MX_OBJ_COMPLETE; return o; }
  if(w->state==MX_LOSE){ o.phase=MX_OBJ_FAILED; return o; }
  if(o.remaining==0&&mx_summit_needed(w)){
    o.phase=w->men<=0&&w->man.mode==MAN_NONE?MX_OBJ_NO_MEN:MX_OBJ_SUMMIT;
  }else if(o.remaining==0){
    o.phase=(w->district+1<mx_level_stage_count(w->level))
      ? MX_OBJ_DISTRICT : MX_OBJ_SECURE;
  }else if(w->men<=0 && w->man.mode==MAN_NONE){
    o.phase=MX_OBJ_NO_MEN;
  }else if(o.remaining==1){
    o.phase=MX_OBJ_LAST;
    if(last->x < w->man.x-RI(2)) o.direction=-1;
    else if(last->x > w->man.x+RI(2)) o.direction=1;
  }
  return o;
}

static inline const char *mx_objective_label(int phase){
  switch(phase){
    case MX_OBJ_SUMMIT: return "CLIMB TO SUMMIT";
    case MX_OBJ_LAST: return "LAST LANDLORD";
    case MX_OBJ_SECURE: return "SECURING AREA";
    case MX_OBJ_DISTRICT: return "NEXT DISTRICT";
    case MX_OBJ_COMPLETE: return "EVICTION COMPLETE";
    case MX_OBJ_FAILED: return "OUT OF MEN";
    case MX_OBJ_NO_MEN: return "NO MEN LEFT";
    default: return "EVICT LANDLORDS";
  }
}
#endif

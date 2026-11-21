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
  MX_OBJ_COMPLETE, MX_OBJ_FAILED, MX_OBJ_NO_MEN
};
typedef struct {
  int phase, remaining, total;
  int direction; /* -1: target to left, +1: target to right, 0: aligned/not needed */
} MxObjective;

static inline MxObjective mx_objective_snapshot(const MxWorld *w){
  MxObjective o = {MX_OBJ_EVICT, 0, w->ntargets, 0};
  const MxTarget *last = 0;
  for(int i=0;i<w->ntargets;i++){
    if(w->targets[i].alive){ o.remaining++; last=&w->targets[i]; }
  }
  if(w->state==MX_WIN){ o.phase=MX_OBJ_COMPLETE; return o; }
  if(w->state==MX_LOSE){ o.phase=MX_OBJ_FAILED; return o; }
  if(o.remaining==0){
    o.phase=(w->level==MX_HQ_LEVEL && w->district<MX_HQ_DISTRICTS-1)
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

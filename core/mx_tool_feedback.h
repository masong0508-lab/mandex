/* Mandex player-facing ability status: read-only, allocation-free.
   Power selection does NOT consume inventory. A use is spent only when
   mx_release() actually launches a shot with that selected power.
   Keep the UI rules in one place for DS / 3DS / Dreamcast. */
#ifndef MX_TOOL_FEEDBACK_H
#define MX_TOOL_FEEDBACK_H
#include "mx.h"

enum {
  MX_SHOT_IDLE, MX_SHOT_PULL, MX_SHOT_VALVE, MX_SHOT_DAZED,
  MX_SHOT_FLY, MX_SHOT_WALK, MX_SHOT_RECOVER, MX_SHOT_FINISH
};
typedef struct {
  int phase, selected, remaining; /* no extra aim/sqrt work in HUD */
} MxToolFeedback;

/* PW_* power names are intentionally distinct from MX_TOOL_* option names\n   in mx_meta.h (whose mx_tool_name has a different index mapping). */
static inline const char *mx_power_name(int id){
  static const char *const names[PW_COUNT] = {
    "NONE", "BEANS", "TOWEL", "SPLAT", "TURBO", "QUAKE"
  };
  return (unsigned)id<PW_COUNT?names[id]:"NONE";
}
/* Short, actionable names for existing powers; shown only while armed.
   All gameplay behavior is already in mx_release / mx_dive. */
static inline const char *mx_tool_effect(int id){
  switch(id){
    case PW_BEANS:return "MID-AIR JET";
    case PW_TOWEL:return "GLIDE FURTHER";
    case PW_SPLAT:return "GOO ON IMPACT";
    case PW_TURBO:return "FASTER LAUNCH";
    case PW_QUAKE:return "LANDING SHOCKWAVE";
    default:return "";
  }
}
static inline int mx_tool_stock(const MxWorld *w,int power){
  if(power<PW_BEANS||power>=PW_COUNT)return 0;
  return w->pw_left[power]>0?w->pw_left[power]:0;
}
/* Visible gauge, measured from the same real sling displacement that
   mx_release() converts to velocity. Calling this is optional for a port:
   only renderers that actually draw a gauge pay for the square root. */
static inline int mx_sling_power_pct(const MxWorld *w){
  const MxMan *m=&w->man;
  if(w->state!=MX_PLAY||m->mode!=MAN_AIM||m->stun>0||
     m->seated||m->vlv)return 0;
  real dx=m->x-MX_ANCHOR_X,dy=m->y-MX_ANCHOR_Y;
  real d2=rmul(dx,dx)+rmul(dy,dy);
  int pct=(int)(rsqrt(d2)*100/MX_PULL_MAX);
  if(pct<0)pct=0;
  if(pct>100)pct=100;
  return pct;
}
static inline MxToolFeedback mx_tool_feedback(const MxWorld *w){
  MxToolFeedback u={MX_SHOT_IDLE,0,0};
  const MxMan *m=&w->man;
  u.selected=w->pw_sel>=PW_BEANS && w->pw_sel<PW_COUNT?
             w->pw_sel:PW_NONE;
  u.remaining=mx_tool_stock(w,u.selected);
  if(w->state!=MX_PLAY){u.phase=MX_SHOT_FINISH;return u;}
  if(m->mode==MAN_WALK){u.phase=MX_SHOT_WALK;return u;}
  if(m->mode==MAN_FLY){u.phase=MX_SHOT_FLY;return u;}
  if(m->mode!=MAN_AIM){u.phase=MX_SHOT_RECOVER;return u;}
  if(m->stun>0){u.phase=MX_SHOT_DAZED;return u;}
  if(m->seated||m->vlv){u.phase=MX_SHOT_VALVE;return u;}
  real dx=m->x-MX_ANCHOR_X,dy=m->y-MX_ANCHOR_Y;
  /* The sling already owns the real power gauge. Use the exact
     mx_release() no-shot threshold here without running sqrt a second time
     in the HUD (important on fixed-point DS). */
  real d2=rmul(dx,dx)+rmul(dy,dy);
  u.phase=d2>=R(3)?MX_SHOT_PULL:MX_SHOT_IDLE;
  return u;
}
static inline const char *mx_tool_status_label(int phase){
  switch(phase){
    case MX_SHOT_PULL:return "RELEASE TO LAUNCH";
    case MX_SHOT_VALVE:return "AIR FILLING - DRAG OFF";
    case MX_SHOT_DAZED:return "DAZED - AIM LOCKED";
    case MX_SHOT_FLY:return "AIRBORNE - TAP TO DIVE";
    case MX_SHOT_WALK:return "ON FOOT - FIGHT OR MOVE";
    case MX_SHOT_RECOVER:return "WAITING FOR NEXT SHOT";
    case MX_SHOT_FINISH:return "LEVEL FINISHED";
    default:return "DRAG BACK TO AIM";
  }
}
#endif

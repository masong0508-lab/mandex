#ifndef MX_OPTIONS_MENU_H
#define MX_OPTIONS_MENU_H
#include "mx_meta.h"
enum { MX_OPT_GAME,MX_OPT_PERF,MX_OPT_AUDIO,MX_OPT_DATA,MX_OPT_PAGES };
enum { MX_O_COLLAPSE,MX_O_FIGHT,MX_O_TOOL,MX_O_GUIDE,MX_O_PROFILE,MX_O_ECO,MX_O_SHAKE,MX_O_MONITOR,MX_O_MUSIC,MX_O_SFX,MX_O_SAVE,MX_O_RESET,MX_O_WIPE };
enum { MX_O_NONE,MX_O_CHANGED,MX_O_SAVE_NOW,MX_O_ERASE };
typedef struct { int page,row,confirm; } MxOptMenu;
static const char *const mx_opt_tabs[4]={"PLAY","PERF","AUDIO","DATA"};
static const char *const mx_opt_names[]={"BUILDING COLLAPSE","LANDLORD AI","START TOOL","AIM GUIDE","PERFORMANCE PROFILE","EFFECTS","SCREEN SHAKE","FPS / SIM METER","MUSIC","SOUND EFFECTS","SAVE NOW","RESET SETTINGS","ERASE PROGRESS"};
static const char *const mx_opt_desc[]={"RUBBLE KEEPS TOPPLING","ENEMIES FIGHT BACK","ARM AVAILABLE POWER ON RELOAD","SHOW BALLISTIC AIM DOTS","ADJUST PRESENTATION ONLY","REDUCE DECORATIVE DRAW WORK","CAMERA IMPACT INTENSITY","SHOW MEASURED FPS AND SIM RATE","PLAY LEVEL MUSIC","PLAY IMPACTS AND MENU SOUNDS","WRITE CURRENT PROGRESS","RESTORE DEFAULTS, KEEP PROGRESS","CLEAR STARS, SCORES AND OUTFITS"};
static const int mx_opt_ids[4][4]={{0,1,2,3},{4,5,6,7},{8,9,-1,-1},{10,11,12,-1}};
static inline int mx_opt_rows(const MxOptMenu *m){return m->page==MX_OPT_AUDIO?2:m->page==MX_OPT_DATA?3:4;}
static inline int mx_opt_id(const MxOptMenu *m,int row){return mx_opt_ids[m->page][row];}
static inline void mx_opt_open(MxOptMenu *m){m->page=0;m->row=0;m->confirm=0;}
static inline void mx_opt_page(MxOptMenu *m,int page){m->page=(page+MX_OPT_PAGES)%MX_OPT_PAGES;m->row=0;m->confirm=0;}
static inline void mx_opt_move(MxOptMenu *m,int dir){m->row=(m->row+dir+mx_opt_rows(m))%mx_opt_rows(m);m->confirm=0;}
static inline void mx_opt_pick(MxOptMenu *m,int row){if(row!=m->row){m->row=row;m->confirm=0;}}
static inline int mx_opt_fx_stride(const MxOptions *o){return o->profile==3?4:(o->eco||o->profile==2)?2:1;}
static inline const char *mx_opt_value(const MxOptions *o,const MxOptMenu *m,int id,int title){
  static const char *const profiles[]={"AUTO","QUALITY","BALANCED","LOW END"};
  static const char *const shakes[]={"FULL","LOW","OFF"};
  switch(id){
  case MX_O_COLLAPSE:return o->collapse?"ON":"OFF";
  case MX_O_FIGHT:return o->nofight?"OFF":"ON";
  case MX_O_TOOL:return mx_tool_name(o->tool);
  case MX_O_GUIDE:return o->no_guide?"OFF":"ON";
  case MX_O_PROFILE:return profiles[o->profile<4?o->profile:0];
  case MX_O_ECO:return o->eco?"ECO":"PROFILE";
  case MX_O_SHAKE:return shakes[o->shake<3?o->shake:0];
  case MX_O_MONITOR:return o->monitor?"ON":"OFF";
  case MX_O_MUSIC:return o->music_off?"OFF":"ON";
  case MX_O_SFX:return o->sfx_off?"OFF":"ON";
  case MX_O_SAVE:return "PRESS A";
  case MX_O_RESET:return m->confirm==MX_O_RESET+1?"CONFIRM":"PRESS A";
  case MX_O_WIPE:return !title?"TITLE ONLY":m->confirm==MX_O_WIPE+1?"CONFIRM":"PRESS A";
  default:return "";
  }
}
static inline int mx_opt_activate(MxOptions *o,MxOptMenu *m,int dir,int title){
  int id=mx_opt_id(m,m->row),step=dir<0?-1:1,prev=m->confirm;
  m->confirm=0;
  if(id>=MX_O_SAVE){
    if(dir)return MX_O_NONE; /* destructive commands require an explicit A/tap */
    if(id==MX_O_SAVE)return MX_O_SAVE_NOW;
    if(id==MX_O_WIPE&&!title)return MX_O_NONE;
    if(prev!=id+1){m->confirm=id+1;return MX_O_NONE;}
    if(id==MX_O_WIPE)return MX_O_ERASE;
    mx_options_default(o);return MX_O_CHANGED;
  }
  switch(id){
    case MX_O_COLLAPSE:o->collapse^=1;break;
    case MX_O_FIGHT:o->nofight^=1;break;
    case MX_O_TOOL:o->tool=(o->tool+step+MX_NUM_TOOLS)%MX_NUM_TOOLS;break;
    case MX_O_GUIDE:o->no_guide^=1;break;
    case MX_O_PROFILE:o->profile=(o->profile+step+4)%4;break;
    case MX_O_ECO:o->eco^=1;break;
    case MX_O_SHAKE:o->shake=(o->shake+step+3)%3;break;
    case MX_O_MONITOR:o->monitor^=1;break;
    case MX_O_MUSIC:o->music_off^=1;break;
    case MX_O_SFX:o->sfx_off^=1;break;
    default:return MX_O_NONE;
  }
  return MX_O_CHANGED;
}
#endif

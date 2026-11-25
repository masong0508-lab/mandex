/*
 * MANDEX core - portable C99 game logic shared by the DS and Dreamcast ports.
 *
 * No platform calls in here: no drawing, no sound, no input, no malloc.
 * Front-ends feed in aim/launch/dive, call mx_update() once per frame, then
 * read the world state and the event list to draw and play sounds.
 *
 *   -DMX_FIXED   20.12 fixed-point (Nintendo DS ARM9 has no FPU)
 *   (default)    float               (Dreamcast SH-4)
 *
 * All units are VOXEL CELLS (1.0 = one voxel), seconds, and cells/second.
 */
#ifndef MX_H
#define MX_H
#include <stdint.h>

/* ---------------------------------------------------------------- real */
#ifdef MX_FIXED
#ifdef MX_LIBNDS_MATH
/* DS: libnds 20.12 helpers use the ARM9's hardware divider and square-root unit
   (REG_DIV / REG_SQRT) instead of slow software 64-bit division. Same format as
   our own helpers below, so results are identical. */
#include <nds.h>
#endif
typedef int32_t real;
#define R(x)   ((real)((x) * 4096.0 + ((x) < 0 ? -0.5 : 0.5)))   /* literals only */
#define RI(i)  ((real)(i) * 4096)
#define RTOF(a) ((float)(a) / 4096.0f)
#ifdef MX_LIBNDS_MATH
static inline real rmul(real a, real b){ return mulf32(a, b); }
static inline real rdiv(real a, real b){ return b ? divf32(a, b) : 0; }
#else
static inline real rmul(real a, real b){ return (real)(((int64_t)a * b) >> 12); }
static inline real rdiv(real a, real b){ return b ? (real)(((int64_t)a * 4096) / b) : 0; }
#endif
static inline int  rfloor(real a){ return a >> 12; }
#else
typedef float real;
#define R(x)   ((real)(x))
#define RI(i)  ((real)(i))
#define RTOF(a) (a)
static inline real rmul(real a, real b){ return a * b; }
static inline real rdiv(real a, real b){ return b != 0 ? a / b : 0; }
int rfloor(real a);
#endif
static inline real rabs(real a){ return a < 0 ? -a : a; }
static inline real rclamp(real v, real lo, real hi){ return v < lo ? lo : v > hi ? hi : v; }
real rsqrt(real a);
real rsin(real a);
real rcos(real a);
real ratan2(real y, real x);                 /* radians, about 0.005 rad accurate, same polynomial in float and fixed so every port agrees */

/* --------------------------------------------------------------- sizes */
#define MX_COLS          120
#define MX_ROWS          60
#ifndef MX_MAX_CHUNKS                    /* rigid bodies at once.  ~12 KB each: the DS keeps 16; with collapsible buildings the 3DS and Dreamcast can afford more (-DMX_MAX_CHUNKS=48) */
#define MX_MAX_CHUNKS    16
#endif
#define MX_CHUNK_CELLS   512
#if MX_MAX_CHUNKS < 4 || MX_MAX_CHUNKS > 63
#error "MX_MAX_CHUNKS must be 4..63 (bnext[] holds int16 cell indexes)"
#endif
#define MX_MAX_TARGETS   20      /* the last Landlord HQ district has 17 */
#define MX_MAX_EVENTS    96
#define MX_NUM_LEVELS    60
#define MX_CH2_START     20      /* chapter 2: levels 20-39 */
#define MX_CH3_START     40      /* chapter 3 (the Wolfman): levels 40-59 */
#define MX_STORY_LEVELS  20      /* chapter 1: the story achievements and outfits count only these, so adding levels never moves earned goalposts */
#define MX_MAX_ROOMS     20

/* Put hot functions in the ARM9's 32 KB instruction TCM (single-cycle, no cache misses).
   Build with -DMX_HOT_ITCM on the DS.  Everywhere else this is empty. */
#ifdef MX_HOT_ITCM
#define MX_HOT __attribute__((section(".itcm"), long_call))
#else
#define MX_HOT
#endif

/* Minimum updates between structural-support passes (0 = as often as needed). */
#ifndef MX_SUPPORT_INTERVAL
#define MX_SUPPORT_INTERVAL 0
#endif

/* Block kinds.  Each has its own mass, strength, stiffness (how it wobbles), damping (how long it
   rings), break strain (how far it can bend before a joint snaps), bounciness and brittleness:
     wood    light, flexible, creaky      brick  heavy, stiff, snaps early    stone  very heavy, very stiff
     glass   light, shatters on impact    tnt    explodes                     steel  heaviest, springy, rings
     rubber  super bouncy; man rebounds   sand   no cohesion: flows like a liquid and piles up            */
enum { MX_EMPTY, MX_WOOD, MX_BRICK, MX_STONE, MX_GLASS, MX_TNT, MX_STEEL, MX_RUBBER, MX_SAND, MX_NMAT };
enum { MX_PLAY, MX_WIN, MX_LOSE };
enum { MAN_NONE, MAN_AIM, MAN_FLY, MAN_STUCK, MAN_WAIT, MAN_WALK };
enum { LK_NONE, LK_WALKER, LK_THROWER, LK_BAT, LK_SHIELD, LK_SPRINTER, LK_BOSS, LK_HEALER };   /* landlord kinds: only act when w->nofight is 0 */

/* Event types the front-end turns into particles and sound. */
enum {
  EV_CELL,     /* a voxel was destroyed at x,y (material m, debris bias a,b) */
  EV_CRUNCH,   /* man plowing through stuff, a = strength 0..1 */
  EV_BOOM,     /* TNT blast at x,y */
  EV_SPLAT,    /* man stuck */
  EV_LAUNCH,
  EV_DIVE,
  EV_THUD,     /* chunk landed, a = strength */
  EV_TARGET,   /* landlord evicted at x,y; b=1 for BANK SHOT bonus */
  EV_POP,      /* man pops away after sticking */
  EV_WIN,
  EV_LOSE,
  EV_CRACK,    /* a joint in the structure snapped at x,y (a = how hard it was strained) */
  EV_BOUNCE,   /* the man rebounded off rubber at x,y, a = speed */
  EV_AIR,      /* hiss of the air valve at x,y, a = inflation 0..1 */
  EV_AIRPOP,   /* he over-inflated and burst at x,y (a man is lost) */
  EV_FART,     /* beans power-up jet at x,y */
  EV_SPLOOSH,  /* splat power-up blast at x,y */
  EV_CRUMBLE,  /* the run ended and the damaged walls crumbled: x,y = centre of the cracked blocks, a = how many (<= 80) */
  EV_LAND,     /* the man touched the ground at x,y: a = speed (cells/s), m = 1 if it was a hard landing that skips (browser: dust, ring, shake) */
  EV_TUBE,     /* he flew into the mouth of pneumatic tube m at x,y and was posted out of its nozzle at (a,b) */
  EV_WALK,     /* walk levels: he landed on his feet at x,y */
  EV_PUNCH,    /* walking punch at x,y: a = damage dealt (0 = whiff), m = 1 if running, b > 0 if PARRY popup takes priority */
  EV_SLAM,     /* walking belly slam at x,y: a = damage dealt, b > 0 if PARRY popup takes priority */
  EV_PAD,      /* bounce pad boing at x,y: m = 1 if it was a stomp-boosted one */
  EV_HURT,     /* a landlord hurt him at x,y: m = 1 on foot, 0 in the sling */
  EV_BONK,     /* bat swat at x,y: m=0 bat, m=1 shield; m=2 player parried a thrown case (+150 points) */
  EV_THROW,    /* a landlord threw a briefcase from x,y */
  EV_SWING,    /* a landlord swung at x,y (walker punch / bat practice swing) */
  EV_HEAL,     /* a healer patched a landlord up at x,y */
  EV_OOF,      /* a tough landlord took a hit and survived at x,y: a = hp left, m = 1 if a shield was smashed */
  EV_DISTRICT, /* Landlord HQ: the district was cleared and the next one is up (m = its number, 1 or 2); +MX_HQ_BONUS_MEN men and +3000 were added */
  EV_SURVIVOR, /* rare, injured escape from a collapsed room; m = remaining HP */
  EV_TNT_FUSE, /* loose TNT armed after a hard impact or nearby blast: x,y precise moving cell */
  EV_TNT_SAFE  /* loose TNT survived a moderate impact without igniting */
};
/* power-ups: one use of each per level */
enum { PW_NONE, PW_BEANS, PW_TOWEL, PW_SPLAT, PW_TURBO, PW_QUAKE, PW_BREAKER, PW_COUNT }; /* appended: preserve legacy save IDs */

typedef struct { uint8_t type, m; real x, y, a, b; } MxEvent;

/* edge = this voxel touches empty space inside its chunk. Interior voxels can't be the first
   point of contact with anything, so the solver skips them (big win on large chunks). */
typedef struct { real lx, ly, wx, wy, h; uint8_t m, dead, edge, fuse; } MxCell; /* fuse uses existing 24-byte padding */

/* Interior rooms (backdrop only): front-ends may draw wallpaper behind the voxels. */
typedef struct { uint8_t x, y, w, h, pal; } MxRoom;

typedef struct {
  uint8_t active;
  int count;
  real x, y, vx, vy, ang, w, m, I, sleep, age, hitCd, rad, cs, sn, cx, cy;
  real px, py, pa;     /* pose at the previous frame, for sleep detection */
  real rx, ry, ra, wt; /* pose at the start of a 1 s window: net displacement catches resting jitter */
  int cn;
  uint8_t rest;        /* collapsible buildings: asleep as a solid body (not integrated, still collides) until something wakes it */
  real restT;          /* seconds asleep; re-checked every 2.4 s + a stagger */
  real tnt_impact;     /* peak exact TNT-voxel collision speed this frame */
  int16_t tnt_hit;     /* index of the actual struck TNT voxel; -1 when none */
  uint8_t has_tnt, tnt_fused, tnt_warned; /* bounded state; no extra arrays or heap */
  MxCell cells[MX_CHUNK_CELLS];
} MxChunk;

/* kind LK_*: walker patrols range cells around home hx and scatters when he flies close, chases and swings at a man on foot; thrower lobs briefcases at the man waiting in the sling;
   bat swats any shot slower than BAT_BREAK.  face = -1/1.  cd = attack cooldown, wind = wind-up left, swing = swing animation left, stun = dazed (bat after a swat), pan = scared (cosmetic), wk = walk phase (cosmetic), mv = 0 still / 1 strolling / 2 running. */
/* chapter 3: hp/hpmax (shield 2, boss 3, others 1; the front-ends draw a bar when hpmax > 1), armor (riot shield facing `face`), inv = hit immunity left,
   shield = blocks a slow shot from the front; sprinter runs, boss throws two briefcases, healer patches the others up; all walkers dodge debris, run for cover and climb one-block stairs. */
typedef struct { real x, y, vy, t; uint8_t alive, kind, mv; int8_t face, pdir; uint8_t eng; real hx, range, cd, wind, swing, stun, pan, wk;
                 uint8_t hp, hpmax, armor, b2; int8_t dd; real inv, dodge, cover, ck, heal;
                 uint8_t room_id, collapse_rolled; /* room index (255=outdoors), 0=eligible, 1=lost roll, 2=escaped once */ } MxTarget;
#define MX_MAX_CASES 4
typedef struct { real x, y, vx, vy, rot, l; uint8_t active; } MxCase;      /* a thrown briefcase */
#define MX_MAX_PADS 3
typedef struct { uint8_t x, y, w; } MxPad;                                  /* bounce pad: top-left cell and width (the pad itself is rubber in the grid) */
#define MX_WALK_T 45

/* Pneumatic tube (chapter 2): a man who flies within MX_TUBE_R of the mouth (ax,ay) is posted out of the nozzle (bx,by) at (vx,vy) cells/s. */
#define MX_MAX_TUBES 3
#define MX_TUBE_R    R(3.75)
typedef struct { real ax, ay, bx, by, vx, vy; } MxTube;

typedef struct {
  int mode;            /* MAN_* */
  real x, y, vx, vy, t, gt, hitCd, gooT, ang;
  /* walk levels: after the flight he lands on his feet (MAN_WALK).  Input comes from mx_walk_input(); wt = seconds left, pT = punch animation left, hurtCd = ouch immunity.
     stun: in the sling, a briefcase hit leaves him dazed (aim is locked) for this long. */
  int8_t face; uint8_t gr, stomp, walked, turn_ticks; /* 12-tick cosmetic side/back/side reverse */
  real wt, pT, pCd, padCd, hurtCd, stun;
  /* Walking forgiveness: short coyote window after leaving a ledge, and
     short-hop release after an ordinary player jump. No new buffers. */
  real ledge_grace, jump_hold;
  real gait;            /* cosmetic ground distance, modulo 4 cells; no renderer memory or save data */
  int8_t in_ax; uint8_t in_jump, in_punch, in_stomp;
  real tubeT, tubeCd;  /* just posted out of a tube: gravity is 10 % for tubeT seconds, no second tube for tubeCd seconds */
  uint8_t dive;
  /* air valve (an unlockable, tucked into the far lower-left corner, clear of every pull-back): w->valve_ok must be set, then
     mx_valve_seat() puts him on it (vlv=1, seated=1) and he inflates; dragging him off (mx_set_pull) fires from the sling as usual.
     inf 0..1: belly size.  At 1 he bursts and the shot is lost.  rad = collision radius, grows with inf. */
  real inf, airT, rad;
  uint8_t vlv, seated, pwr, beaned, splatted, bounces;
  /* body jiggle (cosmetic, driven by his own acceleration): belly and rear are damped springs, in cells.
     Front-ends shift the belly / rear parts of the sprite by (bx,by) / (rx,ry) and squash by sq along (sqx,sqy). */
  real bx, by, bvx, bvy, rx, ry, rvx, rvy;
  real sq, sqx, sqy;
  /* body pose (cosmetic, like the jiggle): in flight ang eases toward 0.6 x the path angle measured as if he faced +x (front-ends mirror
     it when he flies left); spin is the belly-flop's tumble in radians.  Both are 0 outside flight.  mx_man_wobble() is the side-to-side
     rock after he sticks (about +-0.17 rad at first, gone in a second). */
  real spin;
  real j_px, j_py, j_vx, j_vy;       /* previous position / estimated velocity */
} MxMan;

/* Shared contact-correct walk cycle for DS, 3DS and Dreamcast.
   -1 = stationary/airborne (hold the actual side-on pose); 0..3 are
   the successive 1-cell phases of a grounded 4-cell stride. The core
   advances gait only by actual, collision-resolved horizontal motion. */
static inline int mx_walk_anim_phase(const MxMan *m){
  if(m->mode != MAN_WALK || !m->gr || rabs(m->vx) <= R(3)) return -1;
  return rfloor(m->gait) & 3;
}
/* Half-stride lift: continuous 0 -> peak -> 0 in each two-cell step,
   aligned to the EXISTING side-profile sprites. No stored animation state. */
/* A turn follows the existing 15-degree atlas through the back, using
   all six profile-to-rear offsets per side at one sample per simulation tick.
   The mirrored halves meet on the rear silhouette, without new sprites. */
static inline int mx_walk_turn_offset(const MxMan *m){
  if(!m->turn_ticks || m->mode!=MAN_WALK) return 0;
  /* walk_step() decrements turn_ticks before the frame is drawn; at
     the first visible frame it is 11, not 12. Start at the nearest 15deg
     atlas view to avoid a 30deg snap, then pass through the rear view. */
  int step=11-(int)m->turn_ticks;
  if(step<0)step=0;
  if(step>10)step=10;
  return step<6 ? step+1 : 11-step;
}
static inline int mx_walk_render_face(const MxMan *m){
  return m->turn_ticks>6 && m->mode==MAN_WALK?-m->face:m->face;
}
static inline real mx_walk_foot_lift(const MxMan *m){
  if(m->turn_ticks || mx_walk_anim_phase(m)<0) return 0;
  real g=m->gait;
  if(g>=RI(2)) g-=RI(2);
  if(g>RI(1)) g=RI(2)-g;
  return rmul(g,R(0.3));
}
/* A short, purely visual punch lunge with anticipation and recovery.
   It is evaluated from the existing 0.2s punch timer, so every render
   frequency sees the same continuous motion.  No new pose state, body
   collision, save fields or sprites. Peak is 0.30 world cells. */
static inline real mx_walk_punch_lunge(const MxMan *m){
  if(m->mode != MAN_WALK || m->pT <= 0 || m->turn_ticks) return 0;
  real t = rclamp(m->pT, 0, R(0.2));
  if(t > R(0.1)) t = R(0.2) - t;
  return rmul(t, RI(3));
}
/* Shared walking punch rectangle, in world cells. Keep the visible sweep
   tied to these exact gameplay dimensions rather than the delayed sprite
   facing or the scale of the selected character atlas. */
#define MX_WALK_PUNCH_X R(2.125)
#define MX_WALK_PUNCH_Y R(-0.375)
#define MX_WALK_PUNCH_HW R(1.625)
#define MX_WALK_PUNCH_HH RI(2)
/* Draw at most seven samples. Keep a readable cue for the entire live
   parry window, shrinking/fading it as the existing punch timer expires.
   These helpers neither store animation state nor change collision rules. */
static inline int mx_walk_punch_fx_stage(const MxMan *m){
  if(m->mode!=MAN_WALK || m->pT<=0) return 0;
  return m->pT>R(0.13)?3:m->pT>R(0.065)?2:1;
}
static inline void mx_walk_punch_fx_point(const MxMan *m,int k,real *dx,real *dy){
  /* k=-3..3, with exact end points in both numeric modes. */
  const int n=k*k;
  const real arc=n>=9?0:MX_WALK_PUNCH_HW-n*R(1.625/9.0);
  const real x=MX_WALK_PUNCH_X+arc;
  *dx=m->face>0?x:-x;
  *dy=MX_WALK_PUNCH_Y+(k==-3?-MX_WALK_PUNCH_HH:k==3?MX_WALK_PUNCH_HH:k*R(2.0/3.0));
}
/* Avoid single-frame left/right sprite flips when flying near zero horizontal
   speed. The existing face byte is cosmetic until the player walks. */
static inline void mx_flight_face_update(MxMan *m){
  if(m->vx>R(1.5))m->face=1;
  else if(m->vx<-R(1.5))m->face=-1;
}
/* A shared visual intensity scale for aftershock events. A score-derived
   effect should remain capped even if nearly the whole structure collapses. */
static inline int mx_crumble_tier(real cells){
  return cells>=RI(48)?3:cells>=RI(24)?2:cells>=RI(8)?1:0;
}

typedef struct {
  /* world */
  uint8_t grid[MX_ROWS * MX_COLS] __attribute__((aligned(4)));   /* 4-aligned: wob_list scans it a word at a time */
  uint8_t anch[MX_ROWS * MX_COLS];
  real    hp  [MX_ROWS * MX_COLS];
  MxChunk chunks[MX_MAX_CHUNKS];
  MxTarget targets[MX_MAX_TARGETS];
  int ntargets;
  int pw_sel, pw_left[PW_COUNT];        /* armed power-up (PW_*), uses left this level */
  int valve_ok;                  /* air valve unlocked (front-ends set it from mx_valve_unlocked; survives mx_load) */
  int st_inflated, st_pops, st_pw[4];   /* session counters for achievements */
  MxMan man;
  /* game */
  int level, state, men, score;
  /* Evictions within 2.5 real-time seconds stack score bonuses; no allocations or save-format changes. */
  int combo, combo_peak;
  real combo_left;
  real bank_left;               /* 2 seconds after a rubber rebound to earn one +1500 BANK SHOT */
  int bank_contact;              /* transient: only actual flying-man collisions earn the bonus */
  int district;                  /* Landlord HQ (level 39) is three 120-cell districts played one after another; 0..2 (always 0 on other levels) */
  real time, shake, slow, banner, flash, waitT, winT, loseT;
  int freeze, need_support, support_cd, substeps, nhouses;
  int wob_budget, wob_shift;                   /* wobble cost cap: max cells walked per step (0 = unlimited); wob_shift = how much the 'cold' limits are scaled up (0..6) to stay under it */
  /* wobble lattice: every standing voxel is a mass on springs to its 4 neighbours.  Offsets are in cells
     (a voxel is DRAWN at x+ox, y+oy); brk bit d (0:+x 1:-x 2:+y 3:-y) = joint to that neighbour has snapped. */
  real    ox [MX_ROWS * MX_COLS], oy [MX_ROWS * MX_COLS];
  real    ovx[MX_ROWS * MX_COLS], ovy[MX_ROWS * MX_COLS];
  uint8_t brk[MX_ROWS * MX_COLS];
  /* post-run crumble: dmg[i] = a block next to i was destroyed this run.  dz_lo..dz_hi (inclusive cell indices) bounds every cell that may carry a
     dmg flag or a snapped joint, so the once-per-run aftershock scan only walks the rows that matter (empty: dz_lo > dz_hi). */
  uint8_t dmg[MX_ROWS * MX_COLS];
  int dz_lo, dz_hi;
  int collapse;                  /* option: rubble that comes to rest stays a loose solid body until the level ends (see mx_set_collapse) */
  real act_x, act_y;             /* where the last block died: the farthest sleeper is the first to be welded when chunks run short */
  int retry_cd;                  /* frames until the support pass is tried again after it ran out of free chunks (0 = none pending) */
  int wob_enabled, wob_on, wob_quiet, sand_on, sand_t, crack_ev;
  uint32_t stat_cells;          /* chunk voxels processed by the solver (profiling) */
  MxRoom rooms[MX_MAX_ROOMS];
  int nrooms;
  /* Event-driven support for each painted room, no extra framebuffer. */
  uint16_t room_frame_init[MX_MAX_ROOMS];
  uint16_t room_frame_orig[MX_MAX_ROOMS]; /* retained after wallpaper falls: rare collapse-escape odds */
  uint8_t paper_dirty, paper_torn; /* damaged back-wall edges can use subtle rip shading */
  uint8_t paper[MX_ROWS * MX_COLS];   /* wallpaper per cell: palette+1 (0 = none). Blasts, broken walls and collapsing rubble knock it out (3x3 around every voxel lost), so interiors are destructible */
  MxTube tubes[MX_MAX_TUBES];
  int ntubes;
  int walk;                      /* level has walk mode: the man lands on his feet and you steer him */
  int nofight;                   /* option: 1 = landlords just stand there (see mx_set_fight); survives mx_load */
  MxPad pads[MX_MAX_PADS];
  int npads;
  MxCase cases[MX_MAX_CASES];
  uint32_t rng;
  /* events (cleared at the start of each mx_update) */
  MxEvent ev[MX_MAX_EVENTS];
  int nev;
  /* scratch (kept here so the core never needs malloc) */
  uint8_t hb[MX_ROWS * MX_COLS];
  uint8_t vis[MX_ROWS * MX_COLS];
  uint16_t queue[8192];
  uint16_t stack[MX_ROWS * MX_COLS];
  int16_t bhead[(MX_COLS + 4) * (MX_ROWS + 4)];
  int bh_lo, bh_hi;             /* rows of bhead that hold entries (the rest are already -1), so a step clears only those */
  int16_t bnext[MX_MAX_CHUNKS * MX_CHUNK_CELLS];
  int16_t expl[64][2];
  int nexpl;
  /* Display-only status cached during the EXISTING moving-TNT scan.
     Never run another render-time chunk/cell search on small consoles. */
  int tnt_live;               /* number of separately armed moving TNT voxels */
  uint8_t tnt_soonest;        /* ticks to earliest blast, zero when none */
} MxWorld;

/* Anchor of the sling, in cells. */
#define MX_ANCHOR_X R(16)
#define MX_ANCHOR_Y R(45.25)
#define MX_PULL_MAX R(11.5)

#define MX_HQ_LEVEL        39
#define MX_HQ_DISTRICTS    3
#define MX_HQ_BONUS_MEN    4
void mx_load(MxWorld *w, int level);
void mx_update(MxWorld *w, real dt);          /* dt ~ 1/60 s */
void mx_set_pull(MxWorld *w, real dx, real dy); /* drag offset from anchor, cells */
void mx_release(MxWorld *w);                  /* let go of the sling */
real mx_man_wobble(const MxMan *m);          /* stuck man's rock, -1..1: sin(26 t) x exp(-3.2 t), the browser's curve */
void mx_dive(MxWorld *w);                     /* belly flop (or the beans jet when armed) */
void mx_select_power(MxWorld *w, int pw);      /* PW_*: arm / disarm before launching (only while aiming) */
#define MX_VALVE_X R(4.5)                      /* the air valve, in cells (his rear goes here): far lower-left, a full pull-back */
#define MX_VALVE_Y R(52.5)                     /* stays ~1.9 cells clear of it, so no aim can ever touch it */
/* Pointer/touch drag (DS, 3DS): like mx_set_pull, but the raw finger position may be past the full pull. Within MX_VALVE_GRAB of the nozzle he drops onto the
   valve (and stays while the finger stays within MX_VALVE_KEEP); between that and MX_VALVE_PULLIN the sling keeps stretching on toward the valve.  The launch
   power is always capped at a full pull (MX_PULL_MAX), so the extra stretch is only for looks and for finding the seat.  Pad/stick input keeps using mx_set_pull. */
#define MX_VALVE_GRAB   R(5)
#define MX_VALVE_KEEP   R(5.5)
#define MX_VALVE_PULLIN R(9.5)
void mx_drag(MxWorld *w, real dx, real dy);
int  mx_valve_seat(MxWorld *w);
/* Walk levels (w->walk): while man.mode == MAN_WALK the front-end calls this every frame: ax = -1/0/1, the rest are held buttons (jump, punch, stomp), then mx_walk_quit() gives up. */
void mx_walk_input(MxWorld *w, int ax, int jump, int punch, int stomp);
void mx_walk_quit(MxWorld *w);
void mx_set_fight(MxWorld *w, int on);          /* landlords fight back (default on) */
#define MX_BAT_BREAK R(70)                       /* a bat landlord only swats shots slower than this (cells/s) */                /* put him on the valve. 0 if locked or not aiming */
const char *mx_combo_taunt(int count);  /* compact adult-comedy stingers shared by all console HUDs */
int  mx_targets_alive(const MxWorld *w);
int  mx_cells_left(const MxWorld *w);
const char *mx_level_name(int level);
const char *mx_level_tactic(int level); /* read-only stage guidance */
void mx_set_wobble_budget(MxWorld *w, int cells);   /* handhelds: cap the lattice work per frame (0 = off); hits then wake fewer cells instead of stalling the frame */
void mx_set_substeps(MxWorld *w, int n);       /* 2..4: physics steps per frame */
/* Ballistic preview of the current pull: position after t seconds (for aim dots). */
void mx_preview_point(const MxWorld *w, real t, real *ox, real *oy);
const uint8_t *mx_material_rgb(int m);        /* 3 bytes */
int  mx_material_hp(int m);                   /* full health of a block of this kind (for damage tints) */
/* Joint state for drawing cracks: nonzero if any joint of this voxel has snapped. */
/* how far the floor under a landlord has wobbled (so he rides it) */
/* wobble quality switch: 0 = lattice off (cheaper; buildings stay rigid until they break), 1 = on (default) */
void mx_set_wobble(MxWorld *w, int on);
/* collapsible buildings: 1 = fallen rubble stays physical (sleeps as a solid, wakes when something nearby breaks or hits it) until the level ends;
   0 = it welds back into the walls as soon as it stops (the default).  Switching off wakes every sleeper, so it settles the normal way. */
void mx_set_collapse(MxWorld *w, int on);
void mx_target_offset(const MxWorld *w, const MxTarget *t, real *ox, real *oy);
static inline int mx_cell_cracked(const MxWorld *w, int i){ return w->brk[i] != 0; }

#endif

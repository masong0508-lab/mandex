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
#define MX_MAX_TARGETS   12
#define MX_MAX_EVENTS    96
#define MX_NUM_LEVELS    20
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
enum { MAN_NONE, MAN_AIM, MAN_FLY, MAN_STUCK, MAN_WAIT };

/* Event types the front-end turns into particles and sound. */
enum {
  EV_CELL,     /* a voxel was destroyed at x,y (material m, debris bias a,b) */
  EV_CRUNCH,   /* man plowing through stuff, a = strength 0..1 */
  EV_BOOM,     /* TNT blast at x,y */
  EV_SPLAT,    /* man stuck */
  EV_LAUNCH,
  EV_DIVE,
  EV_THUD,     /* chunk landed, a = strength */
  EV_TARGET,   /* landlord evicted at x,y */
  EV_POP,      /* man pops away after sticking */
  EV_WIN,
  EV_LOSE,
  EV_CRACK,    /* a joint in the structure snapped at x,y (a = how hard it was strained) */
  EV_BOUNCE,   /* the man rebounded off rubber at x,y, a = speed */
  EV_AIR,      /* hiss of the air valve at x,y, a = inflation 0..1 */
  EV_AIRPOP,   /* he over-inflated and burst at x,y (a man is lost) */
  EV_FART,     /* beans power-up jet at x,y */
  EV_SPLOOSH,  /* splat power-up blast at x,y */
  EV_CRUMBLE   /* the run ended and the damaged walls crumbled: x,y = centre of the cracked blocks, a = how many (<= 80) */
};
/* power-ups: one use of each per level */
enum { PW_NONE, PW_BEANS, PW_TOWEL, PW_SPLAT };

typedef struct { uint8_t type, m; real x, y, a, b; } MxEvent;

/* edge = this voxel touches empty space inside its chunk. Interior voxels can't be the first
   point of contact with anything, so the solver skips them (big win on large chunks). */
typedef struct { real lx, ly, wx, wy, h; uint8_t m, dead, edge; } MxCell;

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
  MxCell cells[MX_CHUNK_CELLS];
} MxChunk;

typedef struct { real x, y, vy, t; uint8_t alive; } MxTarget;

typedef struct {
  int mode;            /* MAN_* */
  real x, y, vx, vy, t, gt, hitCd, gooT, ang;
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
  real j_px, j_py, j_vx, j_vy;       /* previous position / estimated velocity */
} MxMan;

typedef struct {
  /* world */
  uint8_t grid[MX_ROWS * MX_COLS];
  uint8_t anch[MX_ROWS * MX_COLS];
  real    hp  [MX_ROWS * MX_COLS];
  MxChunk chunks[MX_MAX_CHUNKS];
  MxTarget targets[MX_MAX_TARGETS];
  int ntargets;
  int pw_sel, pw_left[4];        /* armed power-up (PW_*), uses left this level */
  int valve_ok;                  /* air valve unlocked (front-ends set it from mx_valve_unlocked; survives mx_load) */
  int st_inflated, st_pops, st_pw[4];   /* session counters for achievements */
  MxMan man;
  /* game */
  int level, state, men, score;
  real time, shake, slow, banner, flash, waitT, winT, loseT;
  int freeze, need_support, support_cd, substeps, nhouses;
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
  /* events (cleared at the start of each mx_update) */
  MxEvent ev[MX_MAX_EVENTS];
  int nev;
  /* scratch (kept here so the core never needs malloc) */
  uint8_t hb[MX_ROWS * MX_COLS];
  uint8_t vis[MX_ROWS * MX_COLS];
  uint16_t queue[8192];
  uint16_t stack[MX_ROWS * MX_COLS];
  int16_t bhead[(MX_COLS + 4) * (MX_ROWS + 4)];
  int16_t bnext[MX_MAX_CHUNKS * MX_CHUNK_CELLS];
  int16_t expl[64][2];
  int nexpl;
} MxWorld;

/* Anchor of the sling, in cells. */
#define MX_ANCHOR_X R(16)
#define MX_ANCHOR_Y R(45.25)
#define MX_PULL_MAX R(11.5)

void mx_load(MxWorld *w, int level);
void mx_update(MxWorld *w, real dt);          /* dt ~ 1/60 s */
void mx_set_pull(MxWorld *w, real dx, real dy); /* drag offset from anchor, cells */
void mx_release(MxWorld *w);                  /* let go of the sling */
void mx_dive(MxWorld *w);                     /* belly flop (or the beans jet when armed) */
void mx_select_power(MxWorld *w, int pw);      /* PW_*: arm / disarm before launching (only while aiming) */
#define MX_VALVE_X R(4.5)                      /* the air valve, in cells (his rear goes here): far lower-left, a full pull-back */
#define MX_VALVE_Y R(52.5)                     /* stays ~1.9 cells clear of it, so no aim can ever touch it */
int  mx_valve_seat(MxWorld *w);                /* put him on the valve. 0 if locked or not aiming */
int  mx_targets_alive(const MxWorld *w);
int  mx_cells_left(const MxWorld *w);
const char *mx_level_name(int level);
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

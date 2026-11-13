#include "mx.h"
#include <string.h>
#include <math.h>

#define IDX(x, y) ((y) * MX_COLS + (x))
#define INB(x, y) ((x) >= 0 && (x) < MX_COLS && (y) >= 0 && (y) < MX_ROWS)

/* ---- tuning (cell units; 1 cell = 8 px in the HTML prototype) ---- */
#define GRAV      R(112.5)
#define KCOST     R(17.19)      /* energy cost per hp */
#define RC        R(0.5)        /* chunk cell radius */
#define MAN_R0    R(1.75)
#define MAN_R     (m->rad)       /* grows when he is inflated; every user has `MxMan *m` in scope */
#ifndef SUBSTEPS
#define SUBSTEPS  4      /* physics steps per frame; the DS build may lower this */
#endif

/* Per-block-kind physics.
     hp    strength (energy the man must spend to plow through it)
     d     density (mass per voxel)
     lim   how far it can overhang unsupported before the structure gives way
     K     joint stiffness; K/d sets how fast waves run through it (K/d <= ~9000 with 3 sub-steps per frame for stability)
     damp  wobble damping per second: low rings on, high dies quickly
     ymax  break strain: how far (in cells) two neighbours may shear apart before the joint snaps
     rest  bounciness when a loose chunk of it hits something
     fric  friction
     brit  brittleness: how much a chunk impact hurts a voxel of this kind (glass shatters, steel shrugs)  */
typedef struct { int hp; real d; int lim; real K, damp, ymax, rest, fric, brit; } MatProp;
static const MatProp MATS[MX_NMAT] = {
  {0,   R(0),   0,  R(0),     R(0),    R(0),    R(0),    R(0),   R(0)},
  {20,  R(0.6), 6,  R(3600),  R(2.0),  R(0.90), R(0.16), R(0.7), R(0.8)},   /* wood   */
  {40,  R(1.0), 9,  R(8000),  R(3.0),  R(0.30), R(0.12), R(0.8), R(1.3)},   /* brick  */
  {85,  R(1.6), 14, R(12800), R(3.5),  R(0.26), R(0.08), R(0.8), R(0.6)},   /* stone  */
  {5,   R(0.5), 2,  R(4400),  R(3.0),  R(0.12), R(0.30), R(0.3), R(3.0)},   /* glass  */
  {8,   R(0.8), 2,  R(3200),  R(2.0),  R(0.40), R(0.10), R(0.6), R(1.0)},   /* tnt    */
  {150, R(2.0), 16, R(18000), R(1.0), R(1.30), R(0.25), R(0.5), R(0.3)},   /* steel  */
  {60,  R(0.7), 8,  R(450),   R(1.6), R(9.0),  R(0.85), R(1.0), R(0)},     /* rubber */
  {12,  R(1.4), 8,  R(80),    R(7.0),  R(99),   R(0),    R(0.9), R(2.0)}    /* sand   */
};
static const uint8_t MAT_RGB[MX_NMAT][3] = {
  {0,0,0},{185,133,67},{192,80,58},{143,146,156},{174,232,247},{214,42,42},{123,138,156},{255,110,170},{226,196,120}};
int mx_material_hp(int m){ return MATS[m].hp; }
void mx_set_wobble(MxWorld *w, int on){
  w->wob_enabled = on != 0;
  if(!on){                                   /* park everything at rest: snapped joints stay snapped */
    memset(w->ox, 0, sizeof w->ox); memset(w->oy, 0, sizeof w->oy);
    memset(w->ovx, 0, sizeof w->ovx); memset(w->ovy, 0, sizeof w->ovy);
    w->wob_on = 0; w->wob_quiet = 0;
  }
}
void mx_target_offset(const MxWorld *w, const MxTarget *t, real *ox, real *oy){
  *ox = *oy = 0;
  if(!w->wob_on) return;
  int gx = rfloor(t->x), gy = rfloor(t->y) + 1;
  for(int k = 0; k < 3; k++, gy++){                   /* the first solid voxel under his feet */
    if(!INB(gx, gy)) return;
    int i = IDX(gx, gy);
    if(w->grid[i]){ *ox = w->ox[i]; *oy = w->oy[i]; return; }
  }
}
const uint8_t *mx_material_rgb(int m){ return MAT_RGB[m]; }

/* ================================================================ math */
#ifdef MX_FIXED
static int32_t sintab[1024];
static int tables_ready;
static void init_tables(void){
  if(tables_ready) return;
  for(int i = 0; i < 1024; i++) sintab[i] = (int32_t)lroundf(sinf(6.2831853f * i / 1024.0f) * 4096.0f);
  tables_ready = 1;
}
#ifdef MX_LIBNDS_MATH
real rsqrt(real a){ return a > 0 ? sqrtf32(a) : 0; }
#else
real rsqrt(real a){
  if(a <= 0) return 0;
  uint64_t v = (uint64_t)a << 12, r = 0, bit = (uint64_t)1 << 62;
  while(bit > v) bit >>= 2;
  while(bit){ if(v >= r + bit){ v -= r + bit; r = (r >> 1) + bit; } else r >>= 1; bit >>= 2; }
  return (real)r;
}
#endif
static int angidx(real a){ return (int)(((int64_t)a * R(162.9747)) >> 24) & 1023; }
real rsin(real a){ return sintab[angidx(a)]; }
real rcos(real a){ return sintab[(angidx(a) + 256) & 1023]; }
#else
static void init_tables(void){}
int rfloor(real a){ return (int)floorf(a); }
real rsqrt(real a){ return a > 0 ? sqrtf(a) : 0; }
real rsin(real a){ return sinf(a); }
real rcos(real a){ return cosf(a); }
#endif

/* atan2 from one polynomial, built only from rmul / rdiv so float, fixed and the libnds hardware path agree.
   atan(z) ~ z (0.9724 - 0.1919 z^2) on |z| <= 1 (error < 0.005 rad), octant folding for the rest. */
real ratan2(real y, real x){
  real ax = rabs(x), ay = rabs(y);
  if(ax == 0 && ay == 0) return 0;
  int swap = ay > ax;
  real z = swap ? rdiv(ax, ay) : rdiv(ay, ax);
  real a = rmul(z, R(0.9724) - rmul(R(0.1919), rmul(z, z)));
  if(swap) a = R(1.5708) - a;
  if(x < 0) a = R(3.14159) - a;
  return y < 0 ? -a : a;
}

/* ============================================================== events */
static void ev(MxWorld *w, int type, real x, real y, int m, real a, real b){
  if(w->nev >= MX_MAX_EVENTS) return;
  MxEvent *e = &w->ev[w->nev++];
  e->type = (uint8_t)type; e->m = (uint8_t)m; e->x = x; e->y = y; e->a = a; e->b = b;
}

/* ===================================================== destruction core */
static int solid_cell(const MxWorld *w, int x, int y){
  if(y >= MX_ROWS) return 1;
  return INB(x, y) && w->grid[IDX(x, y)];
}
static int solid_at(const MxWorld *w, real px, real py){ return solid_cell(w, rfloor(px), rfloor(py)); }

/* forget every snapped joint touching voxel i (it died, or a new voxel is about to occupy it) */
static void clear_links(MxWorld *w, int i){
  static const int DX4[4] = {1, -1, 0, 0}, DY4[4] = {0, 0, 1, -1};
  int x = i % MX_COLS, y = i / MX_COLS;
  w->brk[i] = 0;
  for(int d = 0; d < 4; d++){
    int nx = x + DX4[d], ny = y + DY4[d];
    if(INB(nx, ny)) w->brk[IDX(nx, ny)] &= (uint8_t)~(1 << (d ^ 1));
  }
}
static void dz_touch(MxWorld *w, int i){ if(i < w->dz_lo) w->dz_lo = i; if(i > w->dz_hi) w->dz_hi = i; }
/* a block died at x,y: its four neighbours are "damaged" and will crumble when the run ends (see aftershock) */
static void mark_dmg(MxWorld *w, int x, int y){
  static const int DX4[4] = {1, -1, 0, 0}, DY4[4] = {0, 0, 1, -1};
  for(int d = 0; d < 4; d++){
    int nx = x + DX4[d], ny = y + DY4[d];
    if(INB(nx, ny)){ int j = IDX(nx, ny); w->dmg[j] = 1; dz_touch(w, j); }
  }
}
static void wob_zero(MxWorld *w, int i){ w->ox[i] = w->oy[i] = w->ovx[i] = w->ovy[i] = 0; }

static void wake_at(MxWorld *w, real px, real py, real r);
static real kill(MxWorld *w, int x, int y, real bvx, real bvy){
  int i = IDX(x, y), m = w->grid[i];
  if(!m) return 0;
  real h = w->hp[i];
  w->grid[i] = 0; w->hp[i] = 0; w->anch[i] = 0; w->need_support = 1; w->score += 10;
  wob_zero(w, i); clear_links(w, i); w->sand_on = 1; mark_dmg(w, x, y);
  if(w->collapse){ w->act_x = RI(x); w->act_y = RI(y); wake_at(w, RI(x) + R(0.5), RI(y) + R(0.5), RI(3)); }
  ev(w, EV_CELL, RI(x) + R(0.5), RI(y) + R(0.5), m, rmul(bvx, R(0.25)), rmul(bvy, R(0.25)));
  if(m == MX_TNT && w->nexpl < 64){ w->expl[w->nexpl][0] = (int16_t)x; w->expl[w->nexpl][1] = (int16_t)y; w->nexpl++; }
  return h;
}
static real damage_cell(MxWorld *w, int x, int y, real d){
  if(!INB(x, y)) return 0;
  int i = IDX(x, y);
  if(!w->grid[i]) return 0;
  if(w->hp[i] <= d) return kill(w, x, y, 0, 0);
  w->hp[i] -= d;
  return d;
}
static real blast(MxWorld *w, int cx, int cy, int r, real power){
  real spent = 0, rr = RI(r);
  for(int j = -r; j <= r; j++) for(int i = -r; i <= r; i++){
    int d2 = i * i + j * j;
    if(d2 > r * r) continue;
    real d = rsqrt(RI(d2));
    real p = rmul(power, RI(1) - rdiv(d, rr));
    if(d2 == 0) p += rmul(power, R(0.25));
    spent += damage_cell(w, cx + i, cy + j, p);
  }
  return spent;
}

static void wake_chunk(MxChunk *ch){ ch->rest = 0; ch->sleep = 0; ch->restT = 0; if(ch->age < R(0.2)) ch->age = R(0.2); }
static void wake_at(MxWorld *w, real px, real py, real r){
  for(int c = 0; c < MX_MAX_CHUNKS; c++){
    MxChunk *ch = &w->chunks[c];
    if(!ch->active || !ch->rest) continue;
    real dx = ch->x - px, dy = ch->y - py, reach = ch->rad + r;
    if(rmul(dx, dx) + rmul(dy, dy) < rmul(reach, reach)) wake_chunk(ch);
  }
}
static int live(const MxWorld *w){ return w->collapse && w->state == MX_PLAY; }
static void apply_impulse(MxChunk *ch, real px, real py, real jx, real jy){
  if(ch->rest) wake_chunk(ch);
  ch->vx += rdiv(jx, ch->m); ch->vy += rdiv(jy, ch->m);
  ch->w  += rdiv(rmul(px - ch->x, jy) - rmul(py - ch->y, jx), ch->I);
  ch->sleep = 0;
}

static void kill_target(MxWorld *w, MxTarget *t){
  if(!t->alive) return;
  t->alive = 0; w->score += 5000; w->slow = R(0.25);
  if(w->man.mode == MAN_WALK){ w->man.wt += RI(3); if(w->man.wt > RI(MX_WALK_T + 15)) w->man.wt = RI(MX_WALK_T + 15); }   /* on foot, every landlord is worth +3 s */
  ev(w, EV_TARGET, t->x, t->y, 0, 0, 0);
}

static void shockwave(MxWorld *w, real px, real py, real rad, real power){
  real reach = rmul(rad, R(1.9));
  for(int c = 0; c < MX_MAX_CHUNKS; c++){
    MxChunk *ch = &w->chunks[c];
    if(!ch->active) continue;
    real dx = ch->x - px, dy = ch->y - py, d = rsqrt(rmul(dx, dx) + rmul(dy, dy));
    if(d <= 0) d = R(0.1);
    if(d > reach) continue;
    real f = rmul(RI(1) - rdiv(d, reach), power);
    apply_impulse(ch, ch->x - rmul(dx, R(0.3)), ch->y - rmul(dy, R(0.3)),
                  rmul(rdiv(dx, d), f), rmul(rdiv(dy, d), f) - rmul(f, R(0.35)));
  }
  for(int i = 0; i < w->ntargets; i++){
    MxTarget *t = &w->targets[i];
    if(!t->alive) continue;
    real dx = t->x - px, dy = t->y - py;
    if(rmul(dx, dx) + rmul(dy, dy) < rmul(rmul(rad, R(1.05)), rmul(rad, R(1.05)))) kill_target(w, t);
  }
}


/* ============================================== wobble lattice
   Every standing voxel is a little mass on springs to its four neighbours (stiffness from the weaker of the
   two kinds).  Hits and blasts inject velocity, which travels through the structure as a wave: stiff heavy
   stone barely moves, wood sways and creaks, steel rings for ages, rubber boings.  Gravity adds an
   inverted-pendulum push (taller = leans further).  When two neighbours shear apart farther than their
   break strain the joint SNAPS; support_pass() then treats the snapped joint as a gap, so a building can
   shear in two along a crack and the upper part topples carrying the velocity it had. */
#define WOB_STEPS 3
#define LEANK     R(0.06)       /* outward accel per cell of sideways offset per cell of height */
#define HOMEK     R(6)          /* weak pull back to the rest position: sheared-off slabs ease home, nothing stays skewed */
#define WOB_VMAX  R(45)
#define WOB_OMAX  R(3)
static real invd[MX_NMAT];
static int mats_ready;
static void init_mats(void){
  if(mats_ready) return;
  for(int m = 1; m < MX_NMAT; m++) invd[m] = rdiv(RI(1), MATS[m].d);
  mats_ready = 1;
}

static void wob_add(MxWorld *w, int i, real vx, real vy){
  if(!w->wob_enabled) return;
  real s = invd[w->grid[i]];
  w->ovx[i] = rclamp(w->ovx[i] + rmul(vx, s), -WOB_VMAX, WOB_VMAX);
  w->ovy[i] = rclamp(w->ovy[i] + rmul(vy, s), -WOB_VMAX, WOB_VMAX);
}
/* a shove (vx,vy cells/s at the centre, fading to nothing at radius r) */
static void wob_kick(MxWorld *w, int cx, int cy, int r, real vx, real vy){
  if(!w->wob_enabled) return;
  for(int j = -r; j <= r; j++) for(int i = -r; i <= r; i++){
    int x = cx + i, y = cy + j, d2 = i * i + j * j;
    if(!INB(x, y) || y >= MX_ROWS - 1 || d2 > r * r) continue;
    int k = IDX(x, y);
    if(!w->grid[k]) continue;
    real f = RI(1) - rdiv(rsqrt(RI(d2)), RI(r + 1));
    wob_add(w, k, rmul(vx, f), rmul(vy, f));
  }
  w->wob_on = 1; w->wob_quiet = 0;
}
/* an outward pulse (explosions) */
static void wob_blast(MxWorld *w, int cx, int cy, int r, real speed){
  if(!w->wob_enabled) return;
  for(int j = -r; j <= r; j++) for(int i = -r; i <= r; i++){
    int x = cx + i, y = cy + j, d2 = i * i + j * j;
    if(!INB(x, y) || y >= MX_ROWS - 1 || d2 > r * r) continue;
    int k = IDX(x, y);
    if(!w->grid[k]) continue;
    real d = rsqrt(RI(d2)), f = RI(1) - rdiv(d, RI(r + 1));
    real dx = d2 ? rdiv(RI(i), d) : 0, dy = d2 ? rdiv(RI(j), d) : -RI(1);
    real v = rmul(speed, f);
    wob_add(w, k, rmul(dx, v), rmul(dy, v));
  }
  w->wob_on = 1; w->wob_quiet = 0;
}

/* The lattice only ever touches standing voxels, so each frame we first list them (one cheap byte scan) and
   the sub-steps then walk that list instead of the whole 120x60 grid. */
#ifndef MX_WOB_STEPS
#define MX_WOB_STEPS WOB_STEPS
#endif
static uint16_t wob_act[MX_ROWS * MX_COLS];
static uint8_t wob_nb[MX_ROWS * MX_COLS];               /* per listed voxel: which of its 4 joints (bit d = +x, -x, +y, -y) are alive this frame */
static uint8_t wob_cx[MX_ROWS * MX_COLS], wob_cy[MX_ROWS * MX_COLS];     /* per listed voxel: column and row (so the steps never divide) */
static real wob_lean[MX_ROWS];                           /* the row's lean spring, which only depends on the height */
static int wob_nact;
static real KK[MX_NMAT][MX_NMAT];                       /* joint stiffness of every pair of kinds (the weaker one) */
/* A voxel that is barely moving, with only such neighbours, has next to no force on it, so the sub-steps skip it (it keeps its tiny offset until the lattice
   goes to sleep and is zeroed).  Anything kicked, blasted or next to a moving voxel is listed. */
#ifndef MX_WOB_COLD_V
#define MX_WOB_COLD_V R(0.05)      /* |vx| + |vy|, cells/s */
#endif
#ifndef MX_WOB_COLD_O
#define MX_WOB_COLD_O R(0.004)     /* |ox| + |oy|, cells */
#endif
static uint8_t wob_hot[MX_ROWS * MX_COLS];             /* by cell index; only read for listed (standing) cells */
static uint16_t occ_i[MX_ROWS * MX_COLS];              /* every standing cell of this frame, row by row */
static uint8_t occ_nb[MX_ROWS * MX_COLS];
static void wob_list(const MxWorld *w){
  int no = 0;
  for(int y = 0; y < MX_ROWS - 1; y++){
    wob_lean[y] = rmul(LEANK, RI(MX_ROWS - 1 - y)) - HOMEK;
    const uint8_t *row = &w->grid[y * MX_COLS];
    for(int x0 = 0; x0 < MX_COLS; x0 += 4){                /* most of the grid is air: look at four cells at a time (MX_COLS is a multiple of 4, grid is 4-aligned) */
      uint32_t q; memcpy(&q, __builtin_assume_aligned(row + x0, 4), 4);
      if(!q) continue;
      for(int x = x0; x < x0 + 4; x++){
        if(!row[x]) continue;
        int i = y * MX_COLS + x;
        uint8_t bk = w->brk[i], nb = 0;                    /* the joints do not change during the sub-steps (wobble_check runs after them), so look them up once */
        if(x + 1 < MX_COLS && !(bk & 1) && w->grid[i + 1]) nb |= 1;
        if(x > 0 && !(bk & 2) && w->grid[i - 1]) nb |= 2;
        if(!(bk & 4) && w->grid[i + MX_COLS]) nb |= 4;
        if(y > 0 && !(bk & 8) && w->grid[i - MX_COLS]) nb |= 8;
        wob_hot[i] = (rabs(w->ovx[i]) + rabs(w->ovy[i]) > MX_WOB_COLD_V) || (rabs(w->ox[i]) + rabs(w->oy[i]) > MX_WOB_COLD_O);
        occ_i[no] = (uint16_t)i; occ_nb[no] = nb; no++;
      }
    }
  }
  int n = 0;
  for(int k = 0; k < no; k++){
    int i = occ_i[k], nb = occ_nb[k];
    if(!(wob_hot[i] || ((nb & 1) && wob_hot[i + 1]) || ((nb & 2) && wob_hot[i - 1]) || ((nb & 4) && wob_hot[i + MX_COLS]) || ((nb & 8) && wob_hot[i - MX_COLS]))) continue;
    wob_act[n] = (uint16_t)i; wob_nb[n] = (uint8_t)nb; wob_cx[n] = (uint8_t)(i % MX_COLS); wob_cy[n] = (uint8_t)(i / MX_COLS); n++;
  }
  wob_nact = n;
}
static MX_HOT void wobble_step(MxWorld *w, real dt){
  real fd[MX_NMAT];
  for(int m = 0; m < MX_NMAT; m++){ fd[m] = RI(1) - rmul(MATS[m].damp, dt); if(fd[m] < 0) fd[m] = 0; }
  if(KK[1][1] == 0) for(int a = 1; a < MX_NMAT; a++) for(int b = 1; b < MX_NMAT; b++) KK[a][b] = MATS[a].K < MATS[b].K ? MATS[a].K : MATS[b].K;
  /* pass 1: forces -> velocities (reads only positions, so the order does not matter) */
  for(int n = 0; n < wob_nact; n++){
    int i = wob_act[n], m = w->grid[i], nb = wob_nb[n];
    real lean = wob_lean[wob_cy[n]];
    real ax = 0, ay = 0, oxi = w->ox[i], oyi = w->oy[i];
    const real *Km = KK[m];
    static const int JD[4] = { 1, -1, MX_COLS, -MX_COLS };
    for(int d = 0; d < 4; d++){                         /* every neighbour pulls in both axes (shear carries the sway up) */
      if(!(nb & (1 << d))) continue;
      int j = i + JD[d];
      real K = Km[w->grid[j]];
      ax += rmul(K, rclamp(w->ox[j] - oxi, -RI(2), RI(2))); ay += rmul(K, rclamp(w->oy[j] - oyi, -RI(2), RI(2)));
    }
    real a = invd[m];
    real vx = w->ovx[i] + rmul(rmul(ax, a) + rmul(lean, oxi), dt);
    real vy = w->ovy[i] + rmul(rmul(ay, a) - rmul(HOMEK, oyi), dt);
    w->ovx[i] = rclamp(rmul(vx, fd[m]), -WOB_VMAX, WOB_VMAX);
    w->ovy[i] = rclamp(rmul(vy, fd[m]), -WOB_VMAX, WOB_VMAX);
  }
  /* pass 2: velocities -> positions */
  for(int n = 0; n < wob_nact; n++){
    int i = wob_act[n];
    w->ox[i] = rclamp(w->ox[i] + rmul(w->ovx[i], dt), -WOB_OMAX, WOB_OMAX);
    w->oy[i] = rclamp(w->oy[i] + rmul(w->ovy[i], dt), -WOB_OMAX, WOB_OMAX);
  }
}
/* once per frame: snap joints that stretched past their break strain, and detect when everything has stopped */
static void wobble_check(MxWorld *w){
  real maxv = 0, maxo = 0;
  for(int n = 0; n < wob_nact; n++){
    int i = wob_act[n], x = wob_cx[n], m = w->grid[i];
    real sv = rabs(w->ovx[i]) + rabs(w->ovy[i]), so = rabs(w->ox[i]) + rabs(w->oy[i]);
    if(sv > maxv) maxv = sv;
    if(so > maxo) maxo = so;
    for(int d = 0; d < 3; d += 2){                       /* each joint once: +x and +y */
      int j = d == 0 ? (x + 1 < MX_COLS ? i + 1 : -1) : i + MX_COLS;
      if(j < 0 || ((w->brk[i] >> d) & 1)) continue;
      int mj = w->grid[j];
      if(!mj) continue;
      real s = rabs(w->ox[j] - w->ox[i]) + rabs(w->oy[j] - w->oy[i]);
      real lim = MATS[m].ymax < MATS[mj].ymax ? MATS[m].ymax : MATS[mj].ymax;
      if(s > lim){
        w->brk[i] |= (uint8_t)(1 << d); w->brk[j] |= (uint8_t)(1 << (d ^ 1));
        dz_touch(w, i); dz_touch(w, j);
        w->need_support = 1;
        if(w->crack_ev < 4){ int y = i / MX_COLS; w->crack_ev++; ev(w, EV_CRACK, RI(x) + R(0.5) + (d == 0 ? R(0.5) : 0), RI(y) + R(0.5) + (d == 2 ? R(0.5) : 0), m, s, 0); }
      }
    }
  }
  if(maxv < R(0.6) && maxo < R(0.03)){
    if(++w->wob_quiet > 20){
      memset(w->ox, 0, sizeof w->ox); memset(w->oy, 0, sizeof w->oy);
      memset(w->ovx, 0, sizeof w->ovx); memset(w->ovy, 0, sizeof w->ovy);
      w->wob_on = 0; w->wob_quiet = 0;
    }
  } else w->wob_quiet = 0;
}

/* ---- sand: no cohesion, so it falls straight down and slides off slopes steeper than 45 degrees ---- */
static void sand_step(MxWorld *w){
  int moved = 0, par = w->sand_t & 1;
  for(int y = MX_ROWS - 2; y >= 0; y--){
    for(int x = 0; x < MX_COLS; x++){
      int i = IDX(x, y);
      if(w->grid[i] != MX_SAND) continue;
      int dst = -1;
      if(!w->grid[i + MX_COLS]) dst = i + MX_COLS;
      else for(int k = 0; k < 2 && dst < 0; k++){
        int nx = x + ((k ^ par) ? 1 : -1);
        if(nx >= 0 && nx < MX_COLS && !w->grid[IDX(nx, y)] && !w->grid[IDX(nx, y + 1)]) dst = IDX(nx, y + 1);
      }
      if(dst < 0) continue;
      w->grid[dst] = MX_SAND; w->hp[dst] = w->hp[i]; w->anch[dst] = 0;
      w->grid[i] = 0; w->hp[i] = 0; w->anch[i] = 0;
      wob_zero(w, i); wob_zero(w, dst); clear_links(w, i); clear_links(w, dst);
      moved = 1;
    }
  }
  w->sand_on = moved;
  if(moved) w->need_support = 1;
}

static void process_expl(MxWorld *w){
  int n = 0;
  while(w->nexpl > 0 && n++ < 80){
    int x = w->expl[0][0], y = w->expl[0][1];
    for(int k = 1; k < w->nexpl; k++){ w->expl[k-1][0] = w->expl[k][0]; w->expl[k-1][1] = w->expl[k][1]; }
    w->nexpl--;
    real px = RI(x) + R(0.5), py = RI(y) + R(0.5);
    blast(w, x, y, 7, R(95));
    shockwave(w, px, py, RI(7), R(650));
    wob_blast(w, x, y, 14, R(26));
    w->shake = R(9); w->slow = R(0.35); w->freeze = 3; w->flash = R(0.35);
    ev(w, EV_BOOM, px, py, 0, 0, 0);
  }
}

/* ---- structural integrity: overhang-limited support, unsupported -> rigid chunk ---- */
static void recompute(MxChunk *ch){
  real M = 0, I = 0;
  for(int i = 0; i < ch->count; i++){
    MxCell *a = &ch->cells[i];
    real d = MATS[a->m].d;
    M += d;
    I += rmul(d, rmul(a->lx, a->lx) + rmul(a->ly, a->ly) + R(0.17));
  }
  if(M < R(0.2)) M = R(0.2);
  if(I < R(1)) I = R(1);
#ifdef MX_FIXED
  if(I > R(400000)) I = R(400000);   /* keep 20.12 headroom for heavy chunks */
#endif
  ch->m = M; ch->I = I;
}

static void settle(MxWorld *w, MxChunk *ch, int keep_upright);
static MxChunk *chunk_slot(MxWorld *w){
  for(int c = 0; c < MX_MAX_CHUNKS; c++) if(!w->chunks[c].active) return &w->chunks[c];
  if(!w->collapse) return 0;
  /* every body is in use: weld the smallest sleeper (the one farthest from the action if two tie) back into the walls and reuse its slot */
  MxChunk *v = 0; real vd = 0;
  for(int c = 0; c < MX_MAX_CHUNKS; c++){
    MxChunk *ch = &w->chunks[c];
    if(!ch->rest) continue;
    real dx = ch->x - w->act_x, dy = ch->y - w->act_y, d = rmul(dx, dx) + rmul(dy, dy);
    if(!v || ch->count < v->count || (ch->count == v->count && d > vd)){ v = ch; vd = d; }
  }
  if(!v) return 0;
  settle(w, v, 0);
  return v;
}

static MX_HOT void support_pass(MxWorld *w){
  w->need_support = 0;
  memset(w->hb, 255, sizeof w->hb);
  unsigned head = 0, tail = 0;
  const unsigned QM = 8191;
  static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
  for(int x = 0; x < MX_COLS; x++){
    int i = IDX(x, MX_ROWS - 1);
    if(w->grid[i]){ w->hb[i] = 0; w->queue[tail++ & QM] = (uint16_t)i; }
  }
  while(head != tail){
    int i = w->queue[head++ & QM], x = i % MX_COLS, y = i / MX_COLS, h = w->hb[i];
    for(int d = 0; d < 4; d++){
      int nx = x + DX[d], ny = y + DY[d];
      if(!INB(nx, ny) || ((w->brk[i] >> d) & 1)) continue;
      int j = IDX(nx, ny), m = w->grid[j];
      if(!m) continue;
      int nh = DY[d] == -1 ? 0 : h + 1;
      if(nh < w->hb[j] && nh <= MATS[m].lim + (w->anch[j] ? 8 : 0) && tail - head < QM){
        w->hb[j] = (uint8_t)nh; w->queue[tail++ & QM] = (uint16_t)j;
      }
    }
  }
  memset(w->vis, 0, sizeof w->vis);
  for(int i = 0; i < MX_ROWS * MX_COLS; i++){
    if(!w->grid[i] || w->grid[i] == MX_SAND || w->hb[i] != 255 || w->vis[i]) continue;
    MxChunk *ch = chunk_slot(w);
    if(!ch) w->retry_cd = 20;            /* no free body right now: stay standing, but look again in a third of a second (a slab must not hang in the air for good) */
    int sp = 0, n = 0, overflow = ch ? 0 : 1;
    w->stack[sp++] = (uint16_t)i; w->vis[i] = 1;
    while(sp){
      int a = w->stack[--sp], x = a % MX_COLS, y = a / MX_COLS;
      if(!overflow){
        if(n < MX_CHUNK_CELLS){
          MxCell *c = &ch->cells[n++];
          c->lx = RI(x); c->ly = RI(y); c->m = w->grid[a]; c->h = w->hp[a]; c->dead = 0;
        } else overflow = 1;
      }
      for(int d = 0; d < 4; d++){
        int nx = x + DX[d], ny = y + DY[d];
        if(!INB(nx, ny)) continue;
        int j = IDX(nx, ny);
        if(w->grid[j] && w->grid[j] != MX_SAND && w->hb[j] == 255 && !w->vis[j] && !((w->brk[a] >> d) & 1)){ w->vis[j] = 1; w->stack[sp++] = (uint16_t)j; }
      }
    }
    if(overflow) continue;               /* too big or no free body: leave it standing */
    /* mark boundary voxels while the component is still in the grid */
    for(int k = 0; k < n; k++){
      MxCell *c = &ch->cells[k];
      int gx = rfloor(c->lx), gy = rfloor(c->ly), e = 0;
      for(int d = 0; d < 4 && !e; d++){
        int nx = gx + DX[d], ny = gy + DY[d];
        if(!INB(nx, ny) || !w->grid[IDX(nx, ny)] || w->hb[IDX(nx, ny)] != 255) e = 1;
      }
#ifdef MX_NO_EDGE
      e = 1;
#endif
      c->edge = (uint8_t)e;
    }
    /* detach: centre of mass, local coords */
    real M = 0, cx = 0, cy = 0, mvx = 0, mvy = 0;
    for(int k = 0; k < n; k++){
      MxCell *c = &ch->cells[k];
      int gi = IDX(rfloor(c->lx), rfloor(c->ly));
      c->lx += w->ox[gi]; c->ly += w->oy[gi];            /* leave from where the wobble has it right now ... */
      c->wx = w->ovx[gi]; c->wy = w->ovy[gi];            /* ... remembering its velocity (temporarily in wx/wy) */
      real d = MATS[c->m].d, px = c->lx + R(0.5), py = c->ly + R(0.5);
      M += d; cx += rmul(d, px); cy += rmul(d, py); mvx += rmul(d, c->wx); mvy += rmul(d, c->wy);
      w->grid[gi] = 0; w->hp[gi] = 0; w->anch[gi] = 0;
      wob_zero(w, gi); clear_links(w, gi);
    }
    cx = rdiv(cx, M); cy = rdiv(cy, M);
    real rad = 0, tq = 0, ti = 0, cvx = rdiv(mvx, M), cvy = rdiv(mvy, M);
    for(int k = 0; k < n; k++){
      MxCell *c = &ch->cells[k];
      real d = MATS[c->m].d, rx = c->lx + R(0.5) - cx, ry = c->ly + R(0.5) - cy;
      tq += rmul(d, rmul(rx, c->wy - cvy) - rmul(ry, c->wx - cvx)); ti += rmul(d, rmul(rx, rx) + rmul(ry, ry));
      c->lx = rx; c->ly = ry; c->wx = c->lx + cx; c->wy = c->ly + cy;
      real r = rsqrt(rmul(c->lx, c->lx) + rmul(c->ly, c->ly));
      if(r > rad) rad = r;
    }
    ch->rest = 0; ch->restT = 0;
    ch->active = 1; ch->count = n; ch->x = cx; ch->y = cy; ch->ang = 0;
    ch->vx = rclamp(cvx, -WOB_VMAX, WOB_VMAX); ch->vy = rclamp(cvy, -WOB_VMAX, WOB_VMAX);
    ch->w = ti > R(1) ? rclamp(rdiv(tq, ti), -RI(6), RI(6)) : 0;   /* the sway becomes a topple */
    ch->sleep = ch->age = ch->hitCd = ch->wt = 0; ch->rad = rad + R(1); ch->cs = RI(1); ch->sn = 0;
    ch->px = cx; ch->py = cy; ch->pa = 0;
    recompute(ch);
  }
}

/* ---- rigid-body chunks ---- */
static void impact_damage(MxWorld *w, MxChunk *ch, MxCell *a, int ci, int cj, real vn){
  real dmg = rmul(vn, R(0.56)) - (a->m == MX_GLASS ? R(4) : R(11.2));
  if(INB(ci, cj) && w->grid[IDX(ci, cj)]) damage_cell(w, ci, cj, rmul(dmg, R(0.9)));
  a->h -= rmul(rmul(dmg, R(0.6)), MATS[a->m].brit);
  if(a->h <= 0){ a->dead = 1; ev(w, EV_CELL, a->wx, a->wy, a->m, 0, 0); }
  real s = rdiv(rmul(vn, ch->m), RI(375));
  ev(w, EV_THUD, a->wx, a->wy, 0, s > RI(1) ? RI(1) : s, 0);
  real sh = rdiv(rmul(vn, ch->m), RI(625));
  if(sh > R(6)) sh = R(6);
  if(sh > w->shake) w->shake = sh;
}

static MX_HOT void resolve_static(MxWorld *w, MxChunk *ch, MxCell *a, real nx, real ny, real pen, int ci, int cj){
  real rx = a->wx - ch->x, ry = a->wy - ch->y;
  real vpx = ch->vx - rmul(ch->w, ry), vpy = ch->vy + rmul(ch->w, rx);
  real vn = rmul(vpx, nx) + rmul(vpy, ny);
  if(vn < 0){
    real rn = rmul(rx, ny) - rmul(ry, nx);
    real den = rdiv(RI(1), ch->m) + rdiv(rmul(rn, rn), ch->I);
    const MatProp *mp = &MATS[a->m];
    real e = (-vn > (a->m == MX_RUBBER ? R(3) : R(16.25))) ? mp->rest : 0;
    real j = rdiv(rmul(-vn, RI(1) + e), den);
    ch->vx += rdiv(rmul(j, nx), ch->m); ch->vy += rdiv(rmul(j, ny), ch->m); ch->w += rdiv(rmul(rn, j), ch->I);
    real tx = -ny, ty = nx;
    real vt = rmul(ch->vx - rmul(ch->w, ry), tx) + rmul(ch->vy + rmul(ch->w, rx), ty);
    real rt = rmul(rx, ty) - rmul(ry, tx);
    real jt = rdiv(-vt, rdiv(RI(1), ch->m) + rdiv(rmul(rt, rt), ch->I));
    real mx_ = rmul(mp->fric, j);
    jt = rclamp(jt, -mx_, mx_);
    ch->vx += rdiv(rmul(jt, tx), ch->m); ch->vy += rdiv(rmul(jt, ty), ch->m); ch->w += rdiv(rmul(rt, jt), ch->I);
    if(-vn > (a->m == MX_GLASS ? R(9) : R(21.25))) impact_damage(w, ch, a, ci, cj, -vn);
    if(-vn > R(7)){                                          /* the thing that got hit wobbles */
      real kf = rclamp(rdiv(ch->m, RI(200)), R(0.03), R(0.4));
      wob_kick(w, ci, cj, 2, rmul(nx, rmul(vn, kf)), rmul(ny, rmul(vn, kf)));
    }
  }
  ch->cx += rmul(nx, pen); ch->cy += rmul(ny, pen); ch->cn++;
}

static MX_HOT void step_chunks(MxWorld *w, real dt){
  int any = 0;
  for(int c = 0; c < MX_MAX_CHUNKS; c++) if(w->chunks[c].active){ any = 1; break; }
  if(!any) return;
  if(w->bh_hi >= w->bh_lo) memset(&w->bhead[w->bh_lo * (MX_COLS + 4)], 0xff, (size_t)(w->bh_hi - w->bh_lo + 1) * (MX_COLS + 4) * sizeof w->bhead[0]);
  int blo = MX_ROWS + 4, bhi = -1;
  for(int c = 0; c < MX_MAX_CHUNKS; c++){
    MxChunk *ch = &w->chunks[c];
    if(!ch->active) continue;
    if(!ch->rest){
      ch->vy += rmul(GRAV, dt);
      ch->vx = rmul(ch->vx, R(0.9995)); ch->w = rmul(ch->w, R(0.998));
      ch->x += rmul(ch->vx, dt); ch->y += rmul(ch->vy, dt); ch->ang += rmul(ch->w, dt);
      ch->cs = rcos(ch->ang); ch->sn = rsin(ch->ang);
    }
    ch->cx = ch->cy = 0; ch->cn = 0;
    for(int i = 0; i < ch->count; i++){
      MxCell *a = &ch->cells[i];
      if(!ch->rest){
        a->wx = ch->x + rmul(ch->cs, a->lx) - rmul(ch->sn, a->ly);
        a->wy = ch->y + rmul(ch->sn, a->lx) + rmul(ch->cs, a->ly);
      }
      if(!a->edge) continue;
      int gx = rfloor(a->wx) + 2, gy = rfloor(a->wy) + 2;
      if(gx < 0) gx = 0;
      if(gx > MX_COLS + 3) gx = MX_COLS + 3;
      if(gy < 0) gy = 0;
      if(gy > MX_ROWS + 3) gy = MX_ROWS + 3;
      int b = gy * (MX_COLS + 4) + gx, id = c * MX_CHUNK_CELLS + i;
      if(gy < blo) blo = gy;
      if(gy > bhi) bhi = gy;
      w->bnext[id] = w->bhead[b]; w->bhead[b] = (int16_t)id;
    }
  }
  w->bh_lo = blo; w->bh_hi = bhi;
  for(int c = 0; c < MX_MAX_CHUNKS; c++){
    MxChunk *ch = &w->chunks[c];
    if(!ch->active || ch->rest) continue;      /* sleepers do no work of their own: moving bodies test against them through the buckets */
    for(int i = 0; i < ch->count; i++){
      MxCell *a = &ch->cells[i];
      if(a->dead || !a->edge) continue;
      w->stat_cells++;
      int gx = rfloor(a->wx), gy = rfloor(a->wy);
      for(int j = gy - 1; j <= gy + 1; j++) for(int k = gx - 1; k <= gx + 1; k++){
        if(!solid_cell(w, k, j)) continue;
        real qx = rclamp(a->wx, RI(k), RI(k + 1)), qy = rclamp(a->wy, RI(j), RI(j + 1));
        real dx = a->wx - qx, dy = a->wy - qy, d2 = rmul(dx, dx) + rmul(dy, dy);
        if(d2 >= rmul(RC, RC)) continue;
        real nx, ny, pen;
        if(d2 > R(0.0005)){
          real d = rsqrt(d2); nx = rdiv(dx, d); ny = rdiv(dy, d); pen = RC - d;
        } else {
          real l = a->wx - RI(k), r = RI(k + 1) - a->wx, t = a->wy - RI(j), b = RI(j + 1) - a->wy, mn = l;
          if(r < mn) mn = r;
          if(t < mn) mn = t;
          if(b < mn) mn = b;
          if(mn == t){ nx = 0; ny = -RI(1); pen = t + RC; }
          else if(mn == l){ nx = -RI(1); ny = 0; pen = l + RC; }
          else if(mn == r){ nx = RI(1); ny = 0; pen = r + RC; }
          else { nx = 0; ny = RI(1); pen = b + RC; }
        }
        if(pen > RI(4)) pen = RI(4);
        resolve_static(w, ch, a, nx, ny, pen, k, j);
      }
      /* chunk vs chunk: circle-circle through the spatial buckets */
      int bx0 = gx + 1, by0 = gy + 1;
      for(int by = by0; by <= by0 + 2; by++) for(int bx = bx0; bx <= bx0 + 2; bx++){
        if(bx < 0 || bx > MX_COLS + 3 || by < 0 || by > MX_ROWS + 3) continue;
        for(int id = w->bhead[by * (MX_COLS + 4) + bx]; id >= 0; id = w->bnext[id]){
          int oc = id / MX_CHUNK_CELLS;
          if(oc == c) continue;
          MxChunk *och = &w->chunks[oc];
          if(oc < c && !och->rest) continue;     /* moving pairs are tested once (from the lower index); a sleeper is tested from whichever moving body reaches it */
          MxCell *o = &och->cells[id % MX_CHUNK_CELLS];
          if(o->dead) continue;
          real dx = a->wx - o->wx, dy = a->wy - o->wy, d2 = rmul(dx, dx) + rmul(dy, dy);
          if(d2 >= RI(1) || d2 < R(0.0005)) continue;
          real d = rsqrt(d2), nx = rdiv(dx, d), ny = rdiv(dy, d), pen = RI(1) - d;
          real rax = a->wx - ch->x, ray = a->wy - ch->y, rbx = o->wx - och->x, rby = o->wy - och->y;
          real vax = ch->vx - rmul(ch->w, ray), vay = ch->vy + rmul(ch->w, rax);
          real vbx = och->vx - rmul(och->w, rby), vby = och->vy + rmul(och->w, rbx);
          real vn = rmul(vax - vbx, nx) + rmul(vay - vby, ny);
          if(vn < 0){
            real rna = rmul(rax, ny) - rmul(ray, nx), rnb = rmul(rbx, ny) - rmul(rby, nx);
            real den, jj;
            if(och->rest){                       /* a sleeper is immovable for a gentle touch (hard ones woke it already) */
              den = rdiv(RI(1), ch->m) + rdiv(rmul(rna, rna), ch->I);
              jj = rdiv(rmul(-vn, R(1.1)), den);
              ch->vx += rdiv(rmul(jj, nx), ch->m); ch->vy += rdiv(rmul(jj, ny), ch->m); ch->w += rdiv(rmul(rna, jj), ch->I);
            } else {
              den = rdiv(RI(1), ch->m) + rdiv(RI(1), och->m) + rdiv(rmul(rna, rna), ch->I) + rdiv(rmul(rnb, rnb), och->I);
              jj = rdiv(rmul(-vn, R(1.1)), den);
              ch->vx += rdiv(rmul(jj, nx), ch->m); ch->vy += rdiv(rmul(jj, ny), ch->m); ch->w += rdiv(rmul(rna, jj), ch->I);
              och->vx -= rdiv(rmul(jj, nx), och->m); och->vy -= rdiv(rmul(jj, ny), och->m); och->w -= rdiv(rmul(rnb, jj), och->I);
            }
            if(-vn > R(25)){
              real dm = rmul(-vn - R(22.5), R(0.04));
              a->h -= dm; o->h -= dm;
              if(a->h <= 0) a->dead = 1;
              if(o->h <= 0) o->dead = 1;
            }
          }
          real tm = ch->m + och->m;
          real fa = rmul(R(0.5), rmul(pen, rdiv(och->m, tm))), fb = rmul(R(0.5), rmul(pen, rdiv(ch->m, tm)));
          if(och->rest){ real full = rmul(R(0.5), pen); ch->cx += rmul(nx, full); ch->cy += rmul(ny, full); ch->cn++; }
          else {
            ch->cx += rmul(nx, fa); ch->cy += rmul(ny, fa); ch->cn++;
            och->cx -= rmul(nx, fb); och->cy -= rmul(ny, fb); och->cn++;
          }
        }
      }
    }
  }
  for(int c = 0; c < MX_MAX_CHUNKS; c++){
    MxChunk *ch = &w->chunks[c];
    if(!ch->active) continue;
    if(ch->cn > 0){
      real s = rdiv(R(0.9), RI(ch->cn));
      ch->x += rmul(ch->cx, s); ch->y += rmul(ch->cy, s);
    }
    ch->age += dt;
  }
}

static const int8_t OFFS[9][2] = {{0,0},{0,-1},{-1,0},{1,0},{0,-2},{-1,-1},{1,-1},{-2,0},{2,0}};
static void settle(MxWorld *w, MxChunk *ch, int keep_upright){
  real ang = keep_upright ? 0 : ch->ang, half = R(1.5708);
  int k90 = rfloor(rdiv(ang, half) + R(0.5));
  real q = rmul(RI(k90), half);
  if(rabs(ang - q) < R(0.4)) ang = q;
  real cs = rcos(ang), sn = rsin(ang);
  for(int i = 0; i < ch->count; i++){
    MxCell *a = &ch->cells[i];
    if(a->dead) continue;
    real wx = ch->x + rmul(cs, a->lx) - rmul(sn, a->ly), wy = ch->y + rmul(sn, a->lx) + rmul(cs, a->ly);
    int gx = rfloor(wx), gy = rfloor(wy), ok = 0;
    for(int o = 0; o < 9 && !ok; o++){
      int X = gx + OFFS[o][0], Y = gy + OFFS[o][1];
      if(INB(X, Y) && !w->grid[IDX(X, Y)]){
        int j = IDX(X, Y); w->grid[j] = a->m; w->hp[j] = a->h; w->anch[j] = 1; w->vis[j] = 1; ok = 1;
        wob_zero(w, j); clear_links(w, j); if(a->m == MX_SAND) w->sand_on = 1;
      }
    }
  }
  ch->active = 0; ch->rest = 0;
  w->need_support = 1;
}

static void clean_chunks(MxWorld *w, real dt){
  for(int c = 0; c < MX_MAX_CHUNKS; c++){
    MxChunk *ch = &w->chunks[c];
    if(!ch->active) continue;
    int lost = 0, n = 0;
    for(int i = 0; i < ch->count; i++){
      if(ch->cells[i].dead){ lost = 1; w->score += 5; continue; }
      if(n != i) ch->cells[n] = ch->cells[i];
      n++;
    }
    if(lost){
      ch->count = n; if(!n){ ch->active = 0; continue; }
      for(int i = 0; i < n; i++) ch->cells[i].edge = 1;   /* interior may now be exposed */
      recompute(ch);
    }
    if(ch->hitCd > 0) ch->hitCd -= dt;
    if(ch->x < RI(-30) || ch->x > RI(150) || ch->y > RI(90)){ ch->active = 0; continue; }
    if(ch->rest){                                       /* collapsible buildings: a sleeper stays solid until something wakes it */
      real spr = rabs(ch->vx) + rabs(ch->vy) + rmul(rabs(ch->w), R(2.25));
      ch->restT += dt;
      if(!live(w) || spr > R(3.75) || ch->restT > R(2.4) + rmul(RI(c % 7), R(0.3))) wake_chunk(ch);
      else { ch->vx = ch->vy = ch->w = 0; ch->px = ch->x; ch->py = ch->y; ch->pa = ch->ang; continue; }
    }
    if(live(w) && rabs(ch->vx) + rabs(ch->vy) > RI(5))     /* a fast body wakes sleepers it is about to hit */
      for(int o = 0; o < MX_MAX_CHUNKS; o++){
        MxChunk *oc = &w->chunks[o];
        if(!oc->active || !oc->rest) continue;
        real dx = oc->x - ch->x, dy = oc->y - ch->y, reach = oc->rad + ch->rad + RI(2);
        if(rmul(dx, dx) + rmul(dy, dy) < rmul(reach, reach)) wake_chunk(oc);
      }
    /* asleep = barely moved this frame (judge displacement, not velocity: resting contacts
       keep a small residual velocity, especially in fixed point) */
    real moved = rabs(ch->x - ch->px) + rabs(ch->y - ch->py) + rmul(rabs(ch->ang - ch->pa), R(2.25));
    ch->px = ch->x; ch->py = ch->y; ch->pa = ch->ang;
    real sp = rdiv(moved, dt);
    /* resting contacts can leave a slow creep that never quite stops (seen: a 29-voxel slab sliding
       ~1 cell/s along the ground for 40 s), so the sleep threshold relaxes with age */
    real thr = RI(1) + rmul(ch->age, R(0.25));
    if(thr > RI(5)) thr = RI(5);
    if(sp < thr && ch->age > R(0.15)){
      ch->sleep += dt;
      if(ch->sleep > R(0.5)){ if(live(w)){ ch->rest = 1; ch->restT = 0; ch->wt = 0; ch->vx = ch->vy = ch->w = 0; } else settle(w, ch, 0); continue; }
    } else ch->sleep = 0;
    /* second test over a whole second: a resting stack can jitter by +-0.1 cell every frame (never
       "slow" frame to frame, e.g. in fixed point with few substeps) yet go nowhere */
    if(ch->wt <= 0){ ch->rx = ch->x; ch->ry = ch->y; ch->ra = ch->ang; }
    ch->wt += dt;
    if(ch->wt >= RI(1)){
      real mv = rabs(ch->x - ch->rx) + rabs(ch->y - ch->ry) + rmul(rabs(ch->ang - ch->ra), R(2.25));
      ch->wt = 0;
      if(mv < R(0.75) && ch->age > RI(1)){ if(live(w)){ ch->rest = 1; ch->restT = 0; ch->vx = ch->vy = ch->w = 0; } else settle(w, ch, 0); continue; }
    }
  }
}

/* ============================================================ aftershock
   When the man's run is over the walls he damaged crumble, as in the browser (aftershock()): every block next to a destroyed one (dmg[])
   and every block with a snapped joint opens a zone (Manhattan distance <= 3 inside a 5x5 window); zone blocks lose their anchor; of those,
   the ones below 70% health (sand excluded) are ranked weakest first and the weakest 80 get all four joints snapped, on both sides of each
   joint.  The support pass then drops whatever is left hanging.  Integer maths on the health ratio, no sort, no malloc: a 70-bucket counting
   pass finds the cut-off health, a second pass takes everything below it (ties come out in scan order, so they may differ from the browser). */
#define CR_REACH 3
#define CR_MAX   80
static int hp_bucket(real h, int mh){                /* -1 if the block is above 70% health, else its health in percent (0..69) */
#ifdef MX_FIXED
  if(h * 10 >= (mh * 7) << 12) return -1;
  int p = (int)((h * 100) / (mh << 12));
#else
  if(h * 10.0f >= mh * 7.0f) return -1;
  int p = (int)(h * 100.0f / mh);
#endif
  return p < 0 ? 0 : p > 69 ? 69 : p;
}
static int aftershock(MxWorld *w, real *cxo, real *cyo){
  static const int DX4[4] = {1, -1, 0, 0}, DY4[4] = {0, 0, 1, -1};
  const int N = MX_ROWS * MX_COLS;
  if(w->dz_hi < w->dz_lo) return 0;
  int lo = w->dz_lo, hi = w->dz_hi, any = 0;
  w->dz_lo = N; w->dz_hi = -1;
  memset(w->vis, 0, sizeof w->vis);                  /* the zone map (vis is scratch: support_pass clears it before it uses it) */
  for(int i = lo; i <= hi; i++){
    if(!w->dmg[i] && !w->brk[i]) continue;
    int x = i % MX_COLS, y = i / MX_COLS;
    for(int j = -2; j <= 2; j++) for(int k = -2; k <= 2; k++){
      if((j < 0 ? -j : j) + (k < 0 ? -k : k) > CR_REACH) continue;
      if(INB(x + k, y + j)){ w->vis[IDX(x + k, y + j)] = 1; any = 1; }
    }
  }
  memset(w->dmg + lo, 0, (size_t)(hi - lo + 1));
  if(!any) return 0;
  int zlo = lo - 2 * MX_COLS - 2, zhi = hi + 2 * MX_COLS + 2;
  if(zlo < 0) zlo = 0;
  if(zhi > N - 1) zhi = N - 1;
  int cnt[70]; memset(cnt, 0, sizeof cnt);
  for(int i = zlo; i <= zhi; i++){
    int m = w->grid[i];
    if(!w->vis[i] || !m || m == MX_SAND) continue;
    w->anch[i] = 0;                                  /* the weld to the ground is gone */
    int b = hp_bucket(w->hp[i], MATS[m].hp);
    if(b >= 0) cnt[b]++;
  }
  int cut = -1, acc = 0;                             /* everything below bucket `cut` is taken; bucket `cut` supplies the rest */
  for(int b = 0; b < 70; b++){ if(acc + cnt[b] >= CR_MAX){ cut = b; break; } acc += cnt[b]; }
  int room = cut >= 0 ? CR_MAX - acc : 0, n = 0, sx = 0, sy = 0;
  for(int i = zlo; i <= zhi; i++){
    int m = w->grid[i];
    if(!w->vis[i] || !m || m == MX_SAND) continue;
    int b = hp_bucket(w->hp[i], MATS[m].hp);
    if(b < 0) continue;
    if(cut >= 0 && (b > cut || (b == cut && room-- <= 0))) continue;
    int x = i % MX_COLS, y = i / MX_COLS;
    for(int d = 0; d < 4; d++){
      int nx = x + DX4[d], ny = y + DY4[d];
      w->brk[i] |= (uint8_t)(1 << d);
      if(INB(nx, ny)) w->brk[IDX(nx, ny)] |= (uint8_t)(1 << (d ^ 1));
    }
    sx += x; sy += y; n++;
  }
  w->need_support = 1;
  if(n){ *cxo = rdiv(RI(sx), RI(n)) + R(0.5); *cyo = rdiv(RI(sy), RI(n)) + R(0.5); }
  return n;
}
/* every real end of a run (he stuck and popped, or flew off the map) comes through here.  Bursting on the air valve is not a run: nothing was hit. */
static void end_run(MxWorld *w){
  w->man.mode = MAN_WAIT; w->waitT = 0;
  real cx = 0, cy = 0;
  int n = aftershock(w, &cx, &cy);
  if(n){
    if(w->shake < R(3)) w->shake = R(3);
    ev(w, EV_CRUMBLE, cx, cy, 0, RI(n), 0);
  }
}

/* bodies that are really moving: every active one, except that with collapsible buildings sleepers and bodies idling in a re-check do not count */
static int busy_count(const MxWorld *w){
  int n = 0;
  for(int c = 0; c < MX_MAX_CHUNKS; c++){ const MxChunk *ch = &w->chunks[c]; if(ch->active && !ch->rest && (!w->collapse || ch->sleep <= 0)) n++; }
  return n;
}
void mx_set_collapse(MxWorld *w, int on){
  w->collapse = on != 0;
  if(!on) for(int c = 0; c < MX_MAX_CHUNKS; c++) if(w->chunks[c].active && w->chunks[c].rest) wake_chunk(&w->chunks[c]);
}

/* ============================================================ the man */
static real damage_cell(MxWorld *w, int x, int y, real d);
static void kill_target(MxWorld *w, MxTarget *t);
static void enter_walk(MxWorld *w);
static int bat_block(MxWorld *w, MxTarget *t);
static void stick(MxWorld *w){
  MxMan *m = &w->man;
  if(m->pwr == PW_SPLAT && !m->splatted){            /* splat power-up: a huge goo blast where he lands */
    m->splatted = 1;
    int cx = rfloor(m->x), cy = rfloor(m->y);
    for(int j = -7; j <= 7; j++) for(int i = -7; i <= 7; i++) if(i * i + j * j <= 49) damage_cell(w, cx + i, cy + j, RI(9));
    for(int i = 0; i < w->ntargets; i++){
      MxTarget *t = &w->targets[i];
      real dx = t->x - m->x, dy = t->y - m->y;
      if(t->alive && rmul(dx, dx) + rmul(dy, dy) < RI(112)) kill_target(w, t);   /* 10.6 cells */
    }
    ev(w, EV_SPLOOSH, m->x, m->y, 0, 0, 0);
    if(w->shake < R(10)) w->shake = R(10);
  }
  if(w->walk && !m->walked){ enter_walk(w); return; }          /* walk levels: he lands on his feet and you steer him */
  m->mode = MAN_STUCK; m->vx = m->vy = 0; m->t = 0; m->gooT = 0;
  if(w->shake < R(4)) w->shake = R(4);
  ev(w, EV_SPLAT, m->x, m->y, 0, 0, 0);
}

/* Rubber does not get plowed: the man rebounds off it (restitution 0.92), and the rubber jiggles. */
static void bounce_rubber(MxWorld *w){
  MxMan *m = &w->man;
  int x0 = rfloor(m->x - MAN_R), x1 = rfloor(m->x + MAN_R), y0 = rfloor(m->y - MAN_R), y1 = rfloor(m->y + MAN_R);
  real rr = rmul(MAN_R, MAN_R);
  for(int y = y0; y <= y1; y++) for(int x = x0; x <= x1; x++){
    if(!INB(x, y) || w->grid[IDX(x, y)] != MX_RUBBER) continue;
    real qx = rclamp(m->x, RI(x), RI(x + 1)), qy = rclamp(m->y, RI(y), RI(y + 1));
    real dx = m->x - qx, dy = m->y - qy, d2 = rmul(dx, dx) + rmul(dy, dy);
    if(d2 >= rr) continue;
    real d = rsqrt(d2), nx, ny;
    if(d > R(0.01)){ nx = rdiv(dx, d); ny = rdiv(dy, d); }
    else {
      real sp = rsqrt(rmul(m->vx, m->vx) + rmul(m->vy, m->vy));
      if(sp <= 0) continue;
      nx = -rdiv(m->vx, sp); ny = -rdiv(m->vy, sp); d = 0;
    }
    m->x += rmul(nx, MAN_R - d); m->y += rmul(ny, MAN_R - d);
    real vn = rmul(m->vx, nx) + rmul(m->vy, ny);
    if(vn < 0){
      real k = rmul(RI(1) + R(0.92), vn);
      m->vx -= rmul(k, nx); m->vy -= rmul(k, ny);
      wob_kick(w, x, y, 5, rmul(nx, rmul(vn, R(0.55))), rmul(ny, rmul(vn, R(0.55))));
      if(m->hitCd <= 0){ m->hitCd = R(0.1); ev(w, EV_BOUNCE, m->x, m->y, 0, -vn, 0); }
      if(w->shake < R(2)) w->shake = R(2);
    }
  }
}

static MX_HOT void man_plow(MxWorld *w){
  bounce_rubber(w);
  MxMan *m = &w->man;
  real mass = rmul(m->dive ? R(2.6) : RI(1), RI(1) + rmul(m->inf, R(1.5)));
  real s2 = rmul(m->vx, m->vx) + rmul(m->vy, m->vy);
  real E = rmul(R(0.5), rmul(mass, s2)), E0 = E;
  int touched = 0;
  int x0 = rfloor(m->x - MAN_R), x1 = rfloor(m->x + MAN_R), y0 = rfloor(m->y - MAN_R), y1 = rfloor(m->y + MAN_R);
  struct { real d2; int x, y; } list[64];
  int n = 0;
  real rr = rmul(MAN_R, MAN_R);
  for(int y = y0; y <= y1; y++) for(int x = x0; x <= x1; x++){
    if(!INB(x, y) || !w->grid[IDX(x, y)] || w->grid[IDX(x, y)] == MX_RUBBER) continue;
    real qx = rclamp(m->x, RI(x), RI(x + 1)), qy = rclamp(m->y, RI(y), RI(y + 1));
    real dx = m->x - qx, dy = m->y - qy, d2 = rmul(dx, dx) + rmul(dy, dy);
    if(d2 >= rr || n >= 64) continue;
    int p = n++;
    while(p > 0 && list[p-1].d2 > d2){ list[p] = list[p-1]; p--; }
    list[p].d2 = d2; list[p].x = x; list[p].y = y;
  }
  if(n) touched = 1;
  static const int DX[4] = {1,-1,0,0}, DY[4] = {0,0,1,-1};
  for(int k = 0; k < n; k++){
    int i = IDX(list[k].x, list[k].y);
    if(!w->grid[i]) continue;
    real cost = rmul(w->hp[i], KCOST);
    if(E >= cost){
      E -= cost;
      real h = kill(w, list[k].x, list[k].y, m->vx, m->vy);
      for(int d = 0; d < 4; d++) damage_cell(w, list[k].x + DX[d], list[k].y + DY[d], rmul(h, R(0.18)));
    } else {
      w->hp[i] -= rdiv(E, KCOST); E = 0; break;
    }
  }
  for(int c = 0; c < MX_MAX_CHUNKS; c++){
    MxChunk *ch = &w->chunks[c];
    if(!ch->active) continue;
    real dx = ch->x - m->x, dy = ch->y - m->y, reach = ch->rad + MAN_R + R(0.5);
    if(rmul(dx, dx) + rmul(dy, dy) > rmul(reach, reach)) continue;
    int hit = 0, cnt = 0; real ax = 0, ay = 0, lim = rmul(MAN_R + RC, MAN_R + RC);
    for(int i = 0; i < ch->count; i++){
      MxCell *a = &ch->cells[i];
      if(a->dead) continue;
      real ddx = a->wx - m->x, ddy = a->wy - m->y;
      if(rmul(ddx, ddx) + rmul(ddy, ddy) >= lim) continue;
      touched = 1; hit = 1; ax += a->wx; ay += a->wy; cnt++;
      real cost = rmul(a->h, KCOST);
      if(E >= cost){ E -= cost; a->dead = 1; ev(w, EV_CELL, a->wx, a->wy, a->m, rmul(m->vx, R(0.25)), rmul(m->vy, R(0.25))); }
      else { a->h -= rdiv(E, KCOST); E = 0; }
    }
    if(hit && ch->hitCd <= 0){
      ch->hitCd = R(0.12);
      apply_impulse(ch, rdiv(ax, RI(cnt)), rdiv(ay, RI(cnt)), rmul(m->vx, rmul(mass, R(0.9))), rmul(m->vy, rmul(mass, R(0.9))));
    }
  }
  for(int i = 0; i < w->ntargets; i++){
    MxTarget *t = &w->targets[i];
    if(!t->alive) continue;
    real dx = t->x - m->x, dy = t->y - m->y, r = MAN_R + R(0.875) + R(0.25);
    if(rmul(dx, dx) + rmul(dy, dy) < rmul(r, r) && !bat_block(w, t)) kill_target(w, t);
  }
  if(touched){
    real spent = E0 - E;
    if(spent > RI(20)){                          /* what the man gave up is shoved into the structure */
      real f = rdiv(spent, RI(2400)); if(f > R(0.55)) f = R(0.55);
      wob_kick(w, rfloor(m->x), rfloor(m->y), 5, rmul(m->vx, f), rmul(m->vy, f));
    }
    if(spent > RI(31) && m->hitCd <= 0){
      m->hitCd = R(0.06);
      real s = rdiv(spent, RI(2500)); if(s > RI(1)) s = RI(1);
      ev(w, EV_CRUNCH, m->x, m->y, 0, s, 0);
      real sh = rdiv(spent, RI(437)); if(sh > RI(9)) sh = RI(9);
      if(sh > w->shake) w->shake = sh;
    }
    if(spent > RI(1719) && w->freeze < 1) w->freeze = 2;
    real s2n = rdiv(rmul(R(2), E), mass), snew = rsqrt(s2n), sold = rsqrt(s2);
    if(sold > 0){ real k = rdiv(snew, sold); m->vx = rmul(m->vx, k); m->vy = rmul(m->vy, k); }
    if(snew < RI(15)) stick(w);
  }
}

static void man_step(MxWorld *w, real dt){
  MxMan *m = &w->man;
  real g = GRAV;
  if(m->pwr == PW_TOWEL && m->gt < R(1.8) && !m->dive) g = rmul(g, R(0.3));      /* towel wings: glide */
  if(m->tubeT > 0) g = rmul(g, R(0.1));                                           /* just out of a tube: nearly weightless for 0.55 s */
  m->vy += rmul(g, dt); m->x += rmul(m->vx, dt); m->y += rmul(m->vy, dt); m->gt += dt;
  if(m->hitCd > 0) m->hitCd -= dt;
  if(w->ntubes){
    if(m->tubeT > 0) m->tubeT -= dt;
    if(m->tubeCd > 0) m->tubeCd -= dt;
    else for(int i = 0; i < w->ntubes; i++){
      const MxTube *tb = &w->tubes[i];
      real dx = m->x - tb->ax, dy = m->y - tb->ay;
      if(rmul(dx, dx) + rmul(dy, dy) > rmul(MX_TUBE_R, MX_TUBE_R)) continue;
      ev(w, EV_TUBE, tb->ax, tb->ay, i, tb->bx, tb->by);
      m->x = tb->bx; m->y = tb->by; m->vx = tb->vx; m->vy = tb->vy; m->ang = 0;
      m->tubeT = R(0.55); m->tubeCd = R(0.35);
      if(w->shake < R(3)) w->shake = R(3);
      break;
    }
  }
  if(m->y + MAN_R >= RI(MX_ROWS)){
    m->y = RI(MX_ROWS) - MAN_R;
    real sp2 = rmul(m->vx, m->vx) + rmul(m->vy, m->vy);
    int skip = sp2 > RI(2756) && m->vy > RI(15) && m->bounces < 2;   /* a hard landing skips once or twice before he sticks (52 cells/s) */
    ev(w, EV_LAND, m->x, RI(MX_ROWS), skip, rsqrt(sp2), 0);
    if(sp2 > RI(1056) && w->shake < R(4)) w->shake = R(4);          /* over 32.5 cells/s (the browser's 260 px/s at 8 px a cell) shakes the screen, skip or stick */
    if(skip){
      m->bounces++; m->vy = -rmul(m->vy, R(0.34)); m->vx = rmul(m->vx, R(0.72));
      ev(w, EV_THUD, m->x, m->y, 0, R(0.5), 0);
      return;
    }
    stick(w);
    return;
  }
  man_plow(w);
}

static void next_man(MxWorld *w){
  memset(&w->man, 0, sizeof w->man);
  if(w->men > 0){ w->man.mode = MAN_AIM; w->man.x = MX_ANCHOR_X; w->man.y = MX_ANCHOR_Y; }
  else w->man.mode = MAN_NONE;
  w->man.rad = MAN_R0;
  for(int i = 0; i < w->ntargets; i++) if(w->targets[i].kind == LK_THROWER && w->targets[i].cd < R(2.6)) w->targets[i].cd = R(2.6);   /* a fresh man in the sling gets a grace period */
}

void mx_set_pull(MxWorld *w, real dx, real dy){
  MxMan *m = &w->man;
  if(m->mode != MAN_AIM || m->stun > 0) return;
  real d = rsqrt(rmul(dx, dx) + rmul(dy, dy));
  if(d > MX_PULL_MAX){ real k = rdiv(MX_PULL_MAX, d); dx = rmul(dx, k); dy = rmul(dy, k); }
  m->x = MX_ANCHOR_X + dx; m->y = MX_ANCHOR_Y + dy;
  m->vlv = 0; m->seated = 0;                            /* any pull-back leaves the valve; the valve never grabs a pull */
}
void mx_drag(MxWorld *w, real dx, real dy){
  MxMan *m = &w->man;
  if(m->mode != MAN_AIM || m->stun > 0) return;
  real fx = MX_ANCHOR_X + dx - MX_VALVE_X, fy = MX_ANCHOR_Y + dy - MX_VALVE_Y, dv2 = rmul(fx, fx) + rmul(fy, fy);
  if(w->valve_ok && w->state == MX_PLAY){
    if(m->seated && dv2 < rmul(MX_VALVE_KEEP, MX_VALVE_KEEP)) return;               /* finger still on the seat: stay */
    if(!m->seated && dv2 < rmul(MX_VALVE_GRAB, MX_VALVE_GRAB)){ mx_valve_seat(w); return; }
  }
  mx_set_pull(w, dx, dy);
  if(w->valve_ok && dv2 < rmul(MX_VALVE_PULLIN, MX_VALVE_PULLIN)){               /* nearing the valve: the sling stretches on back toward it */
    real dv = rsqrt(dv2), k = rdiv(MX_VALVE_PULLIN - dv, MX_VALVE_PULLIN - MX_VALVE_GRAB);
    k = rmul(k, R(0.9));
    m->x += rmul(MX_VALVE_X - m->x, k); m->y += rmul(MX_VALVE_Y - m->y, k);
  }
}
int mx_valve_seat(MxWorld *w){
  MxMan *m = &w->man;
  if(!w->valve_ok || m->mode != MAN_AIM || w->state != MX_PLAY || m->stun > 0) return 0;
  m->x = MX_VALVE_X; m->y = MX_VALVE_Y; m->vlv = 1; m->seated = 1;
  return 1;
}
void mx_release(MxWorld *w){
  MxMan *m = &w->man;
  if(m->mode != MAN_AIM || w->state != MX_PLAY || m->stun > 0) return;
  if(m->vlv){ m->seated = 1; return; }                  /* let go ON the valve: he stays and keeps filling; drag him off to fire */
  real dx = MX_ANCHOR_X - m->x, dy = MX_ANCHOR_Y - m->y;
  if(rmul(dx, dx) + rmul(dy, dy) < R(3)){ m->x = MX_ANCHOR_X; m->y = MX_ANCHOR_Y; return; }
  { real d = rsqrt(rmul(dx, dx) + rmul(dy, dy));                   /* stretched on toward the valve: power is still a full pull at most */
    if(d > MX_PULL_MAX){ real k = rdiv(MX_PULL_MAX, d); dx = rmul(dx, k); dy = rmul(dy, k); m->x = MX_ANCHOR_X - dx; m->y = MX_ANCHOR_Y - dy; } }
  { real sp0 = RI(1) - rmul(m->inf, R(0.5));            /* inflated: slower but bigger and heavier */
    m->rad = rmul(MAN_R0, RI(1) + rmul(m->inf, R(0.9)));
    m->vx = rmul(rmul(dx, R(9.2)), sp0); m->vy = rmul(rmul(dy, R(9.2)), sp0);
    if(m->inf > R(0.6)) w->st_inflated++;
    if(w->pw_sel){ m->pwr = (uint8_t)w->pw_sel; w->pw_left[w->pw_sel]--; w->st_pw[w->pw_sel]++; w->pw_sel = 0; } }
  m->mode = MAN_FLY; m->gt = 0; w->men--;
  m->spin = 0; m->ang = rmul(ratan2(m->vy, rabs(m->vx)), R(0.6));
  ev(w, EV_LAUNCH, m->x, m->y, 0, 0, 0);
}
void mx_select_power(MxWorld *w, int pw){
  if(w->man.mode != MAN_AIM || pw < PW_BEANS || pw > PW_SPLAT || w->pw_left[pw] <= 0) return;
  w->pw_sel = (w->pw_sel == pw) ? 0 : pw;
}
void mx_dive(MxWorld *w){
  MxMan *m = &w->man;
  if(m->mode == MAN_FLY && m->pwr == PW_BEANS && !m->beaned){     /* beans: a fart jet along his flight */
    m->beaned = 1;
    real sp = rsqrt(rmul(m->vx, m->vx) + rmul(m->vy, m->vy));
    if(sp > 0){ m->vx += rmul(rdiv(m->vx, sp), RI(65)); m->vy += rmul(rdiv(m->vy, sp), RI(65)) - RI(10); }
    ev(w, EV_FART, m->x, m->y, 0, 0, 0);
    if(w->shake < R(5)) w->shake = R(5);
    return;
  }
  if(m->mode != MAN_FLY || m->dive) return;
  m->dive = 1; m->vx = rmul(m->vx, R(0.4));
  if(m->vy < 0) m->vy = 0;
  m->vy += R(97.5);
  if(w->shake < R(3)) w->shake = R(3);
  ev(w, EV_DIVE, m->x, m->y, 0, 0, 0);
}

/* ========================================================== landlords */
/* ---------------------------------------------------------------- chapter 2: landlords fight back, and walking
   Same rules as the web game (px / 8 = cells).  Landlord kinds only act while w->nofight is 0 and the level is being played. */
#define WHW R(0.875)            /* walking man: half width 7 px */
#define WHH R(2.375)            /* half height 19 px */
static real rrand(MxWorld *w){                       /* 0 <= r < 1, deterministic per level load */
  w->rng = w->rng * 1664525u + 1013904223u;
  uint32_t v = (w->rng >> 16) & 4095;
#ifdef MX_FIXED
  return (real)v;
#else
  return (real)v * (1.0f / 4096.0f);
#endif
}
static int sgn(real a){ return a > 0 ? 1 : a < 0 ? -1 : 0; }
void mx_set_fight(MxWorld *w, int on){ w->nofight = !on; }

/* a landlord's blow lands on the man: on foot he loses 3 s and is knocked back, in the sling he is dazed for 1.4 s and loses 2000 points */
static int hurt_man(MxWorld *w, int dir){
  MxMan *m = &w->man;
  if(m->hurtCd > 0) return 0;
  if(m->mode == MAN_WALK){
    m->hurtCd = R(0.8); m->wt -= RI(3); if(m->wt < 0) m->wt = 0;
    m->vx = RI(dir * 35); m->vy = R(-26.25); m->gr = 0;
  } else if(m->mode == MAN_AIM){
    m->hurtCd = R(0.8); m->seated = m->vlv = 0; m->x = MX_ANCHOR_X; m->y = MX_ANCHOR_Y; m->stun = R(1.4);
    w->score -= 2000; if(w->score < 0) w->score = 0;
  } else return 0;
  ev(w, EV_HURT, m->x, m->y, m->mode == MAN_WALK, 0, 0);
  if(w->shake < RI(8)) w->shake = RI(8);
  return 1;
}

/* a lob from the landlord's hands to (tx,ty) that clears the walls; 0 if none of four flight times works */
static int throw_sol(MxWorld *w, const MxTarget *t, real tx, real ty, real *ovx, real *ovy){
  static const real TT[4] = {R(0.85), R(1.05), R(1.3), R(1.6)};
  int dx = tx > t->x ? 1 : tx < t->x ? -1 : t->face;
  real sx = t->x + (dx > 0 ? R(1.125) : -R(1.125)), sy = t->y - R(1.125), hg = rmul(R(0.5), GRAV);
  for(int k = 0; k < 4; k++){
    real T = TT[k], vx = rdiv(tx - sx, T), vy = rdiv(ty - sy - rmul(hg, rmul(T, T)), T);
    if(rmul(vx, vx) + rmul(vy, vy) > RI(15006)) continue;                      /* faster than 122.5 cells/s: he cannot throw that hard */
    int ok = 1;
    for(real u = R(0.07); u < T - R(0.04); u += R(0.03))
      if(solid_at(w, sx + rmul(vx, u), sy + rmul(vy, u) + rmul(hg, rmul(u, u)))){ ok = 0; break; }
    if(ok){ *ovx = vx; *ovy = vy; return 1; }
  }
  return 0;
}

static void lord_ai(MxWorld *w, MxTarget *t, real dt){
  MxMan *m = &w->man;
  int here = m->mode == MAN_AIM || m->mode == MAN_FLY || m->mode == MAN_WALK || m->mode == MAN_STUCK;
  real mx_ = here ? m->x : MX_ANCHOR_X, my_ = here ? m->y : MX_ANCHOR_Y;
  real ddx = mx_ - t->x, ddy = my_ - t->y, tm = rsqrt(rmul(ddx, ddx) + rmul(ddy, ddy));
  int fight = !w->nofight && w->state == MX_PLAY && t->kind;
  if(t->stun > 0) t->stun -= dt;
  if(t->swing > 0) t->swing -= dt;
  if(t->pan > 0) t->pan -= dt;
  if(m->mode == MAN_FLY && tm < R(26.25)) t->pan = R(0.7);          /* everybody flinches when a man comes screaming close (cosmetic) */
  int moving = 0;
  if(fight){
    if(t->kind == LK_THROWER){ if(m->mode == MAN_AIM && !(m->stun > 0)) t->cd -= dt; } else t->cd -= dt;
    if(t->wind > 0){
      t->wind -= dt;
      if(t->wind <= 0){
        if(t->kind == LK_THROWER){
          real vx, vy;
          if(m->mode == MAN_AIM && throw_sol(w, t, m->x, m->y, &vx, &vy)){
            for(int k = 0; k < MX_MAX_CASES; k++) if(!w->cases[k].active){
              MxCase *c = &w->cases[k];
              c->active = 1; c->x = t->x + (t->face > 0 ? R(1.125) : -R(1.125)); c->y = t->y - R(1.125); c->vx = vx; c->vy = vy; c->rot = 0; c->l = RI(4);
              ev(w, EV_THROW, c->x, c->y, 0, 0, 0);
              break;
            }
            t->swing = R(0.3);
          }
          t->cd = R(4.2) + rmul(rrand(w), R(1.6));
        } else if(t->kind == LK_WALKER){
          t->swing = R(0.3);
          ev(w, EV_SWING, t->x, t->y, 0, 0, 0);
          if(m->mode == MAN_WALK && rabs(ddx) < R(5.75) && rabs(ddy) < R(5.75)) hurt_man(w, sgn(m->x - t->x) ? sgn(m->x - t->x) : 1);
        }
      }
    } else if(t->kind == LK_THROWER && m->mode == MAN_AIM && !(m->stun > 0) && t->cd <= 0){
      real vx, vy;
      if(throw_sol(w, t, m->x, m->y, &vx, &vy)){ t->wind = R(0.6); if(sgn(m->x - t->x)) t->face = (int8_t)sgn(m->x - t->x); }
      else t->cd = R(0.7);
    }
    if(t->kind == LK_WALKER && t->wind <= 0 && t->stun <= 0){
      int dir = 0; real sp = R(3.25);
      int flying = m->mode == MAN_FLY && tm < RI(35);
      int foot = m->mode == MAN_WALK && rabs(ddx) < RI(42) + R(0.5) && rabs(ddy) < R(6.25);
      if(!foot) t->eng = 0;
      if(foot){
        if(!t->eng){ t->eng = 1; if(t->cd > R(0.6)) t->cd = R(0.6); }
        dir = sgn(ddx); sp = RI(9);
        if(rabs(ddx) < R(4.75)){ dir = 0; if(t->cd <= 0){ t->wind = R(0.36); t->cd = R(1.2); } }       /* stops just out of bump range and winds up */
      } else if(flying){ dir = sgn(t->x - m->x); if(!dir) dir = 1; sp = R(11.25); }
      else {
        if(!t->pdir) t->pdir = t->face;
        dir = t->pdir;
        if(rabs(t->x - t->hx) > t->range && sgn(t->x - t->hx) == dir){ t->pdir = (int8_t)(-dir); dir = -dir; }
      }
      if(dir){
        real ax = t->x + (dir > 0 ? R(1.125) : -R(1.125));
        int gr = solid_at(w, t->x, t->y + R(1.0));
        int blocked = solid_at(w, ax, t->y) || solid_at(w, ax, t->y - R(0.625)) || !solid_at(w, ax, t->y + R(1.25));
        if(gr && !blocked){ t->x += rmul(dir > 0 ? sp : -sp, dt); t->wk += rmul(dt, sp); moving = sp > R(6.25) ? 2 : 1; }
        else if(!flying && !foot){ t->pdir = (int8_t)(-dir); dir = -dir; }
        t->face = (int8_t)dir;
      }
    } else if(t->kind == LK_BAT && t->stun <= 0 && m->mode == MAN_FLY && tm < R(18.75) && t->swing <= 0){
      t->swing = R(0.35); ev(w, EV_SWING, t->x, t->y, 1, 0, 0);
    }
  }
  if(!moving && !(t->swing > 0) && t->wind <= 0 && sgn(mx_ - t->x)) t->face = (int8_t)sgn(mx_ - t->x);
  t->mv = (uint8_t)moving;
}

/* a bat landlord swats a man arriving slower than MX_BAT_BREAK: he rebounds, the bat is dazed for 1.1 s.  Returns 1 if the swat happened (no eviction). */
static int bat_block(MxWorld *w, MxTarget *t){
  MxMan *m = &w->man;
  if(t->kind != LK_BAT || w->nofight || w->state != MX_PLAY || t->stun > 0) return 0;
  real sp = rsqrt(rmul(m->vx, m->vx) + rmul(m->vy, m->vy));
  if(sp >= MX_BAT_BREAK) return 0;
  real nx = m->x - t->x, ny = m->y - t->y - RI(1), d = rsqrt(rmul(nx, nx) + rmul(ny, ny));
  if(d <= 0) d = RI(1);
  nx = rdiv(nx, d); ny = rdiv(ny, d);
  real out = rmul(sp, R(0.8)); if(out < R(47.5)) out = R(47.5);
  m->vx = rmul(nx, out) + (m->vx < 0 ? 0 : -RI(30)); m->vy = rmul(ny, out) - RI(15);
  m->x = t->x + rmul(nx, MAN_R + R(1.5)); m->y = t->y + rmul(ny, MAN_R + R(1.5));
  t->stun = R(1.1); t->swing = R(0.35); if(sgn(m->x - t->x)) t->face = (int8_t)sgn(m->x - t->x);
  if(w->shake < RI(7)) w->shake = RI(7);
  ev(w, EV_BONK, t->x, t->y, 0, 0, 0);
  return 1;
}

static void upd_cases(MxWorld *w, real dt){
  MxMan *m = &w->man;
  for(int k = 0; k < MX_MAX_CASES; k++){
    MxCase *c = &w->cases[k];
    if(!c->active) continue;
    c->vy += rmul(GRAV, dt); c->x += rmul(c->vx, dt); c->y += rmul(c->vy, dt); c->rot += rmul(c->vx > 0 ? RI(9) : -RI(9), dt); c->l -= dt;
    int dead = c->l <= 0 || c->y > RI(MX_ROWS) - R(0.5) || c->x < -R(2.5) || c->x > RI(MX_COLS) + R(2.5);
    if(!dead && solid_at(w, c->x, c->y)) dead = 1;
    if(!dead && (m->mode == MAN_AIM || m->mode == MAN_WALK)){
      real dx = m->x - c->x, dy = m->y - c->y, lim = m->mode == MAN_WALK ? R(2.75) : m->rad + R(0.75);
      if(rmul(dx, dx) + rmul(dy, dy) < rmul(lim, lim) && hurt_man(w, c->vx < 0 ? -1 : 1)) dead = 1;
    }
    if(dead){ c->active = 0; ev(w, EV_THUD, c->x, c->y, 1, R(0.4), 0); }
  }
}

/* ---- walk mode ---- */
static int wblocked(const MxWorld *w, real x, real y){
  const real xs[3] = { x - WHW, x, x + WHW }, ys[5] = { y - WHH, y - R(1.1875), y, y + R(1.1875), y + WHH - R(0.0125) };
  for(int i = 0; i < 3; i++) for(int j = 0; j < 5; j++) if(solid_at(w, xs[i], ys[j])) return 1;
  for(int c = 0; c < MX_MAX_CHUNKS; c++){
    const MxChunk *ch = &w->chunks[c];
    if(!ch->active || rabs(ch->x - x) > ch->rad + RI(2) || rabs(ch->y - y) > ch->rad + RI(3)) continue;
    for(int k = 0; k < ch->count; k++){
      const MxCell *a = &ch->cells[k];
      if(a->dead || rabs(a->wx - x) > WHW + RC || rabs(a->wy - y) > WHH + RC) continue;
      for(int i = 0; i < 3; i++) for(int j = 0; j < 5; j++) if(rabs(a->wx - xs[i]) < RC && rabs(a->wy - ys[j]) < RC) return 1;
    }
  }
  return 0;
}
#define PX(n) rmul(RI(n), R(0.125))
static void enter_walk(MxWorld *w){
  MxMan *m = &w->man;
  m->face = m->vx < 0 ? -1 : 1; m->mode = MAN_WALK; m->walked = 1; m->vx = m->vy = 0; m->gr = 0; m->wt = RI(MX_WALK_T); m->pT = m->pCd = 0; m->stomp = 0;
  m->in_ax = 0; m->in_jump = m->in_punch = m->in_stomp = 0;
  int ok = !wblocked(w, m->x, m->y);
  for(int u = 1; u <= 110 && !ok; u++) if(!wblocked(w, m->x, m->y - PX(u))){ m->y -= PX(u); ok = 1; }
  for(int r = 4; r <= 200 && !ok; r += 4) for(int a = 0; a < 16 && !ok; a++){           /* nearest free spot, preferring up */
    real an = -R(1.5708) + (a & 1 ? 1 : -1) * ((a + 1) / 2) * R(0.4);
    real qx = m->x + rmul(rcos(an), PX(r)), qy = m->y + rmul(rsin(an), PX(r));
    if(!wblocked(w, qx, qy)){ m->x = qx; m->y = qy; ok = 1; }
  }
  ev(w, EV_WALK, m->x, m->y, 0, 0, 0);
  if(w->shake < RI(4)) w->shake = RI(4);
}
static void walk_end(MxWorld *w){ ev(w, EV_POP, w->man.x, w->man.y, 0, 0, 0); end_run(w); }
void mx_walk_quit(MxWorld *w){ if(w->man.mode == MAN_WALK && w->state == MX_PLAY) walk_end(w); }
void mx_walk_input(MxWorld *w, int ax, int jump, int punch, int stomp){
  MxMan *m = &w->man;
  m->in_ax = (int8_t)(ax > 0 ? 1 : ax < 0 ? -1 : 0); m->in_jump = (uint8_t)(jump != 0); m->in_punch = (uint8_t)(punch != 0); m->in_stomp = (uint8_t)(stomp != 0);
}
/* hit everything in a box (cells, loose rubble, landlords); returns the damage dealt.  Bounce pads shrug off fists. */
static real walk_hit(MxWorld *w, real cx, real cy, real hw, real hh, real dmg){
  real dealt = 0;
  int x0 = rfloor(cx - hw), x1 = rfloor(cx + hw), y0 = rfloor(cy - hh), y1 = rfloor(cy + hh);
  for(int y = y0; y <= y1; y++) for(int x = x0; x <= x1; x++){ if(INB(x, y) && w->grid[IDX(x, y)] == MX_RUBBER) continue; dealt += damage_cell(w, x, y, dmg); }
  for(int c = 0; c < MX_MAX_CHUNKS; c++){
    MxChunk *ch = &w->chunks[c];
    if(!ch->active || rabs(ch->x - cx) > ch->rad + hw + hh || rabs(ch->y - cy) > ch->rad + hw + hh) continue;
    int hit = 0;
    for(int k = 0; k < ch->count; k++){
      MxCell *a = &ch->cells[k];
      if(a->dead || rabs(a->wx - cx) >= hw + RC || rabs(a->wy - cy) >= hh + RC) continue;
      hit = 1; dealt += a->h < dmg ? a->h : dmg; a->h -= dmg;
      if(a->h <= 0){ a->dead = 1; ev(w, EV_CELL, a->wx, a->wy, a->m, 0, 0); }
    }
    if(hit) wake_chunk(ch);
  }
  for(int i = 0; i < w->ntargets; i++){
    MxTarget *t = &w->targets[i];
    if(t->alive && rabs(t->x - cx) < hw + R(0.875) && rabs(t->y - cy) < hh + R(0.875)) kill_target(w, t);
  }
  return dealt;
}
static void walk_punch(MxWorld *w){
  MxMan *m = &w->man;
  real cx = m->x + (m->face > 0 ? R(2.125) : -R(2.125)), cy = m->y - R(0.375);
  int run = rabs(m->vx) > R(12.5);
  real d = walk_hit(w, cx, cy, R(1.625), RI(2), RI(run ? 24 : 16));
  m->pT = R(0.2); m->pCd = R(0.3);
  ev(w, EV_PUNCH, cx, cy, run, d, 0);
  if(d > 0){ if(w->shake < RI(3)) w->shake = RI(3); wob_kick(w, rfloor(cx), rfloor(cy), 4, m->face > 0 ? R(0.9) : -R(0.9), 0); }
}
static void walk_slam(MxWorld *w){
  MxMan *m = &w->man;
  real cx = m->x, cy = m->y + WHH + R(0.5) + R(0.5), d = 0;
  d += walk_hit(w, cx, cy, R(1.75), R(1.25), RI(34));
  d += walk_hit(w, cx, cy, R(3.25), R(1.25), RI(18));
  m->stomp = 0;
  if(w->shake < RI(9)) w->shake = RI(9);
  ev(w, EV_SLAM, cx, cy, 0, d, 0);
  wob_kick(w, rfloor(cx), rfloor(cy), 6, 0, R(0.9));
}
static void walk_step(MxWorld *w, real dt){
  MxMan *m = &w->man;
  m->wt -= dt;
  if(m->wt <= 0){ walk_end(w); return; }
  if(wblocked(w, m->x, m->y)){                         /* rubble fell on him: pop him out */
    int f = 0;
    for(int u = 2; u <= 80 && !f; u += 2) for(int k = 0; k < 3 && !f; k++){
      int sg = k == 0 ? 0 : k == 1 ? -1 : 1;
      if(!wblocked(w, m->x + sg * PX(u), m->y - PX(u))){ m->x += sg * PX(u); m->y -= PX(u); f = 1; }
    }
  }
  int ax = m->in_ax;
  if(ax) m->face = (int8_t)ax;
  { real tv = ax > 0 ? R(16.875) : ax < 0 ? -R(16.875) : 0, k = rmul(dt, m->gr ? RI(14) : RI(5)); if(k > RI(1)) k = RI(1); m->vx += rmul(tv - m->vx, k); }
  if(m->pCd > 0) m->pCd -= dt;
  if(m->pT > 0) m->pT -= dt;
  if(m->in_punch && m->pCd <= 0) walk_punch(w);
  if(m->in_jump && m->gr && !m->stomp){ m->vy = R(-43.125); m->gr = 0; }
  if(m->in_stomp && !m->gr && !m->stomp){ m->stomp = 1; if(m->vy < RI(90)) m->vy = RI(90); }
  { real dx = rmul(m->vx, dt); int nx = rfloor(rdiv(rabs(dx), R(0.25))) + 1; if(nx > 12) nx = 12;
    real sx = rdiv(dx, RI(nx));
    for(int i = 0; i < nx; i++){
      if(!wblocked(w, m->x + sx, m->y)) m->x += sx;
      else {
        int ok = 0;
        if(!m->gr && m->vy > R(-47.5)) for(int u = 1; u <= 10; u++) if(!wblocked(w, m->x + sx, m->y - PX(u))){ m->x += sx; m->y -= PX(u); if(m->vy > RI(-15)) m->vy = RI(-15); ok = 1; break; }   /* airborne next to a ledge: hop up onto it */
        if(!ok && m->gr) for(int u = 1; u <= 9; u++) if(!wblocked(w, m->x + sx, m->y - PX(u))){ m->x += sx; m->y -= PX(u); ok = 1; break; }                 /* a stair is one cell: he steps up it */
        if(!ok){ m->vx = 0; break; }
      }
    }
  }
  m->x = rclamp(m->x, WHW + R(0.25), RI(MX_COLS) - WHW - R(0.25));
  m->vy += rmul(rmul(GRAV, R(0.9)), dt);
  { real dy = rmul(m->vy, dt); int ny = rfloor(rdiv(rabs(dy), R(0.25))) + 1; if(ny > 12) ny = 12;
    real sy = rdiv(dy, RI(ny));
    for(int i = 0; i < ny; i++){
      if(!wblocked(w, m->x, m->y + sy)){ m->y += sy; continue; }
      if(sy > 0){
        if(m->stomp){ walk_slam(w); if(!wblocked(w, m->x, m->y + sy)){ m->y += sy; m->vy = rmul(m->vy, R(0.8)); continue; } }
        if(m->vy > R(47.5)) ev(w, EV_THUD, m->x, m->y + WHH, 0, R(0.5), 0);
        m->vy = 0;
      } else {
        if(rabs(dy) > R(0.25)) ev(w, EV_THUD, m->x, m->y - WHH, 0, R(0.3), 0);
        m->vy = 0;
      }
      break;
    }
  }
  m->gr = (uint8_t)wblocked(w, m->x, m->y + R(0.1875));
  if(m->gr) m->stomp = 0;
  if(m->padCd > 0) m->padCd -= dt;
  if(m->gr && !(m->padCd > 0)){
    int by = rfloor(m->y + WHH + R(0.25));
    for(int k = 0; k < 3; k++){
      int bx = rfloor(m->x + (k == 0 ? -R(0.5) : k == 1 ? 0 : R(0.5)));
      if(INB(bx, by) && w->grid[IDX(bx, by)] == MX_RUBBER){
        m->vy = R(-58.75); m->gr = 0; m->stomp = 0; m->padCd = R(0.3);
        ev(w, EV_PAD, m->x, m->y + WHH, 0, 0, 0);
        break;
      }
    }
  }
  for(int i = 0; i < w->ntargets; i++){                   /* bump into a landlord and he is done for */
    MxTarget *t = &w->targets[i];
    real dx = t->x - m->x, dy = t->y - m->y, lim = WHH + R(0.875) - R(0.5);
    if(t->alive && rmul(dx, dx) + rmul(dy, dy) < rmul(lim, lim)) kill_target(w, t);
  }
}

static void upd_targets(MxWorld *w, real dt){
  for(int i = 0; i < w->ntargets; i++){
    MxTarget *t = &w->targets[i];
    if(!t->alive) continue;
    t->t += dt;
    lord_ai(w, t, dt);
    real r = R(0.875);
    int gr = solid_at(w, t->x, t->y + r + R(0.125)) || solid_at(w, t->x - R(0.5), t->y + r + R(0.125)) || solid_at(w, t->x + R(0.5), t->y + r + R(0.125));
    if(gr){ if(t->vy > R(41)){ kill_target(w, t); continue; } t->vy = 0; }
    else { t->vy += rmul(GRAV, dt); t->y += rmul(t->vy, dt); }
    if(solid_at(w, t->x, t->y)){ kill_target(w, t); continue; }
    for(int c = 0; c < MX_MAX_CHUNKS && t->alive; c++){
      MxChunk *ch = &w->chunks[c];
      if(!ch->active) continue;
      real dx = ch->x - t->x, dy = ch->y - t->y, reach = ch->rad + r + R(0.5);
      if(rmul(dx, dx) + rmul(dy, dy) > rmul(reach, reach)) continue;
      for(int k = 0; k < ch->count; k++){
        MxCell *a = &ch->cells[k];
        real ddx = a->wx - t->x, ddy = a->wy - t->y, lim = r + RC;
        if(rmul(ddx, ddx) + rmul(ddy, ddy) >= rmul(lim, lim)) continue;
        real vx = ch->vx - rmul(ch->w, a->wy - ch->y), vy = ch->vy + rmul(ch->w, a->wx - ch->x);
        if(rmul(vx, vx) + rmul(vy, vy) > R(76.6)){ kill_target(w, t); break; }
      }
    }
  }
}

/* ============================================================= levels */
static void set_cell(MxWorld *w, int x, int y, int m){
  if(!INB(x, y)) return;
  int i = IDX(x, y);
  w->grid[i] = (uint8_t)m; w->hp[i] = RI(MATS[m].hp); w->anch[i] = 0;
}
static void rect_fill(MxWorld *w, int x, int y, int ww, int hh, int m){
  for(int j = 0; j < hh; j++) for(int i = 0; i < ww; i++) set_cell(w, x + i, y + j, m);
}
/* fh = floor height, wt = wall thickness, ft = thickness of the upper floor slabs (they grow DOWN so floor tops stay put);
   same layout rules as the web game's house() so every port builds the same building */
static void house_ex(MxWorld *w, int x, int g, int ww, int floors, int fh, int wt, int ft, int wall, int floor, int roof, int gable, int crenel){
  const int top = g + 1 - (floors * fh + 1);
  for(int f = 0; f <= floors; f++) rect_fill(w, x, g - f * fh, ww, f ? ft : 1, floor);
  for(int f = 0; f < floors; f++){
    int yb = g - f * fh, y0 = yb - fh + ft;
    if(w->nrooms < MX_MAX_ROOMS){
      MxRoom *rm = &w->rooms[w->nrooms++];
      rm->x = (uint8_t)(x + wt); rm->y = (uint8_t)y0; rm->w = (uint8_t)(ww - 2 * wt); rm->h = (uint8_t)(fh - ft); rm->pal = (uint8_t)(w->nhouses & 3);
    }
    for(int y = y0; y < yb; y++){
      for(int k = 0; k < wt; k++){ set_cell(w, x + k, y, wall); set_cell(w, x + ww - 1 - k, y, wall); }
      if(ww > 13) set_cell(w, x + (ww >> 1), y, wall == MX_STEEL ? MX_STEEL : MX_WOOD);
    }
    for(int y = yb - 5; y < yb - 3; y++) if(y >= y0){ set_cell(w, x, y, MX_GLASS); set_cell(w, x + ww - 1, y, MX_GLASS); }
  }
  if(gable){ for(int k = 0; k < 7 && ww + 2 - 2 * k > 1; k++) rect_fill(w, x - 1 + k, top - 1 - k, ww + 2 - 2 * k, 1, roof); }
  else rect_fill(w, x - 1, top - 2, ww + 2, 2, roof);
  if(crenel) for(int k = 0; k < ww + 2; k += 2) set_cell(w, x - 1 + k, top - 3, roof);
  w->nhouses++;
}
static void house_at(MxWorld *w, int x, int g, int ww, int floors, int wall, int floor, int roof, int gable, int crenel){
  house_ex(w, x, g, ww, floors, 9, 1, 1, wall, floor, roof, gable, crenel);
}
static void house(MxWorld *w, int x, int ww, int floors, int wall, int floor, int roof, int gable, int crenel){
  house_at(w, x, MX_ROWS - 1, ww, floors, wall, floor, roof, gable, crenel);
}
/* a truncated sand pyramid (45 degree slopes are what sand can hold) */
static void mound(MxWorld *w, int x, int width, int height){
  for(int r = 0; r < height && width - 2 * r > 0; r++) rect_fill(w, x + r, MX_ROWS - 1 - r, width - 2 * r, 1, MX_SAND);
}
/* pneumatic tube: mouth (ax,ay) and nozzle (bx,by) in cells; the nozzle shoots straight along +x at 95 cells/s (the web game's 760 px/s) */
static void tube(MxWorld *w, real ax, real ay, real bx, real by){
  if(w->ntubes >= MX_MAX_TUBES) return;
  MxTube *t = &w->tubes[w->ntubes++];
  t->ax = ax; t->ay = ay; t->bx = bx; t->by = by; t->vx = R(95); t->vy = 0;
}
/* the hourglass of Hourglass Heights: bore half-width d rows from the neck, a 9-row neck, glass to just under the waist, steel below, sand above the neck */
static int hg_hw(int d){ int v = 3 + (d * 7) / 10; return v > 14 ? 14 : v; }
static void hourglass(MxWorld *w, int c, int rn, int top, int bot, int sand_top){
  for(int r = top; r <= bot; r++){
    int d = r > rn ? r - rn : rn - r; d = d > 4 ? d - 4 : 0;
    int hw = hg_hw(d), glass = r <= rn + 2, wm = glass ? MX_GLASS : MX_STEEL, th = glass ? 2 : 3;
    for(int k = 0; k < th; k++){ set_cell(w, c - hw - 1 - k, r, wm); set_cell(w, c + hw + k, r, wm); }
    if(r >= sand_top && r < rn - 4) for(int x = c - hw; x < c + hw; x++) set_cell(w, x, r, MX_SAND);
  }
}
static void tnt(MxWorld *w, int x, int y){ rect_fill(w, x, y - 2, 2, 2, MX_TNT); }
static void tgtk(MxWorld *w, int cx, int row, int kind){
  if(w->ntargets >= MX_MAX_TARGETS) return;
  MxTarget *t = &w->targets[w->ntargets++];
  t->x = RI(cx) + R(0.5); t->y = RI(row) - R(0.875); t->vy = 0; t->t = 0; t->alive = 1;
  t->kind = (uint8_t)kind; t->hx = t->x; t->range = RI(7); t->face = rrand(w) < R(0.5) ? -1 : 1; t->cd = RI(2) + rrand(w) * 2;
}
static void tgt(MxWorld *w, int cx, int row){ tgtk(w, cx, row, LK_NONE); }
static void room(MxWorld *w, int x, int y, int ww, int hh, int pal){
  if(w->nrooms >= MX_MAX_ROOMS) return;
  MxRoom *rm = &w->rooms[w->nrooms++];
  rm->x = (uint8_t)x; rm->y = (uint8_t)y; rm->w = (uint8_t)ww; rm->h = (uint8_t)hh; rm->pal = (uint8_t)(pal & 3);
}
static void pad(MxWorld *w, int x, int y, int ww){
  if(w->npads >= MX_MAX_PADS) return;
  MxPad *p = &w->pads[w->npads++]; p->x = (uint8_t)x; p->y = (uint8_t)y; p->w = (uint8_t)ww;
}

static const char *NAMES[MX_NUM_LEVELS] = {
  "Suburban Eviction", "Duplex Dilemma", "Tower Block Tussle", "Castle Foreclosure", "Corporate Takeover",
  "Bouncy Castle", "Sandcastle Shakedown",
  "Glass House Party", "Steel Mill Standoff", "Twin Peaks", "Domino Drive", "Rubber Ducky Resort", "TNT Pyramid",
  "Skyscraper Special", "Bridge Troll", "Fort Knoxed", "Jenga Joe", "Moat Madness", "Glass Cannon", "Mandex Manor",
  "Special Delivery", "Hourglass Heights", "Walk-Up Wreckage", "Over the Top", "Needle Threader", "Collection Day", "Batter Up",
  "Leaning Loft", "Powder Keg Plaza", "Rubber Room", "Stairway to Eviction", "Sand Trap", "The Vault", "Triple Decker",
  "Glass Menagerie", "Penthouse Panic", "Landlord HQ"};
static const int MEN[MX_NUM_LEVELS] = {5, 5, 6, 6, 7, 6, 7,  5, 7, 6, 6, 7, 6, 8, 8, 8, 7, 7, 6, 10,  6, 6, 4, 6, 6, 6, 6,  6, 6, 8, 8, 6, 7, 8, 6, 8, 12};
const char *mx_level_name(int l){ return NAMES[l % MX_NUM_LEVELS]; }

static void build(MxWorld *w, int l){
  const int W_ = MX_WOOD, B = MX_BRICK, S = MX_STONE, ST = MX_STEEL, RU = MX_RUBBER;
  switch(l){
  /* levels 0-19: the same layouts as the web game (house_ex args: x, ground row, width, floors, floor height, wall thickness, slab thickness, wall, floor, roof, gable, crenel) */
#define HX(x, g, ww, fl, fh, wt, ft, wl, fo, ro, ga, cr) house_ex(w, x, g, ww, fl, fh, wt, ft, wl, fo, ro, ga, cr)
#define GR (MX_ROWS - 1)
  case 0:       /* wide low house + a little wooden shed */
    HX(74, GR, 18, 2, 8, 1, 1, B, W_, W_, 1, 0); HX(60, GR, 8, 1, 8, 1, 1, W_, W_, W_, 0, 0);
    tgt(w, 64, 59); tgt(w, 80, 59); tgt(w, 89, 51); tnt(w, 85, 59); break;
  case 1:       /* a tall skinny wooden tower beside a long low brick block */
    HX(55, GR, 8, 3, 11, 1, 1, W_, W_, W_, 1, 0); HX(70, GR, 26, 2, 7, 2, 1, B, W_, B, 0, 0);
    tgt(w, 59, 59); tgt(w, 59, 48); tgt(w, 59, 37); tgt(w, 76, 59); tgt(w, 90, 52); tnt(w, 84, 59); tnt(w, 57, 48); break;
  case 2:       /* seven squat floors with double-thick stone slabs */
    HX(60, GR, 10, 2, 9, 1, 1, W_, W_, W_, 1, 0); HX(80, GR, 14, 7, 7, 1, 2, B, S, S, 0, 0);
    tgt(w, 64, 59); tgt(w, 65, 50); tgt(w, 85, 59); tgt(w, 90, 45); tgt(w, 84, 31); tgt(w, 90, 17); tnt(w, 88, 31); tnt(w, 90, 52); break;
  case 3:       /* 2-cell stone walls and slabs: a long keep between tall slim towers */
    HX(52, GR, 8, 5, 9, 2, 2, S, S, S, 1, 0); HX(64, GR, 26, 3, 8, 2, 2, S, S, S, 0, 1); HX(92, GR, 8, 4, 9, 2, 2, S, S, S, 1, 0);
    tgt(w, 55, 41); tgt(w, 68, 59); tgt(w, 74, 51); tgt(w, 84, 59); tgt(w, 95, 32); tgt(w, 55, 23); tnt(w, 80, 59); tnt(w, 70, 43); break;
  case 4:       /* steel office on chunky stone slabs, plus a brick annex */
    HX(50, GR, 12, 3, 9, 1, 1, B, W_, B, 1, 0); HX(64, GR, 6, 4, 8, 1, 1, B, B, B, 0, 0); HX(76, GR, 20, 5, 8, 1, 2, ST, S, ST, 0, 0);
    tgt(w, 55, 59); tgt(w, 58, 50); tgt(w, 67, 43); tgt(w, 80, 59); tgt(w, 90, 51); tgt(w, 84, 43); tgt(w, 92, 35); tgt(w, 82, 27);
    tnt(w, 88, 59); tnt(w, 80, 35); tnt(w, 92, 27); break;
  case 5:       /* rubber walls: the man rebounds, so come in through the wooden roofs or use TNT */
    HX(52, GR, 16, 3, 8, 2, 1, RU, W_, W_, 1, 0); HX(76, GR, 12, 5, 7, 1, 2, RU, B, W_, 0, 0);
    tgt(w, 58, 59); tgt(w, 63, 51); tgt(w, 58, 43); tgt(w, 80, 59); tgt(w, 84, 52); tgt(w, 80, 45); tgt(w, 84, 31);
    tnt(w, 62, 59); tnt(w, 84, 38); break;
  case 6:       /* a deep sand mound under a squat stone house, a shallow one under a tall brick tower: dig out the foundations */
    mound(w, 44, 34, 10); HX(54, 49, 12, 2, 8, 2, 1, S, W_, S, 1, 0);
    mound(w, 84, 32, 8);  HX(92, 51, 14, 3, 7, 1, 1, B, W_, W_, 1, 0);
    tgt(w, 62, 49); tgt(w, 58, 41); tgt(w, 96, 51); tgt(w, 100, 44); tgt(w, 96, 37); tgt(w, 80, 59); tnt(w, 60, 49); tnt(w, 100, 51); break;
  case 7:       /* glass tower of low floors, and a wide glass hall on thick stone slabs */
    HX(58, GR, 10, 6, 6, 1, 1, MX_GLASS, W_, W_, 0, 0); HX(80, GR, 18, 2, 12, 1, 3, MX_GLASS, S, S, 0, 0);
    tgt(w, 62, 59); tgt(w, 64, 53); tgt(w, 62, 41); tgt(w, 63, 29); tgt(w, 86, 59); tgt(w, 92, 47); tgt(w, 84, 47);
    tnt(w, 66, 59); tnt(w, 90, 59); break;
  case 8:       /* thick steel mill, a tall steel smokestack with a wooden cap, a long low brick shed */
    HX(60, GR, 16, 3, 9, 2, 2, ST, B, W_, 0, 0); rect_fill(w, 82, 26, 4, 34, ST); rect_fill(w, 80, 24, 8, 2, W_);
    HX(90, GR, 14, 1, 8, 1, 1, B, W_, W_, 0, 0);
    tgt(w, 65, 59); tgt(w, 70, 50); tgt(w, 65, 41); tgt(w, 68, 30); tgt(w, 84, 24); tgt(w, 95, 59); tgt(w, 97, 49);
    tnt(w, 66, 50); tnt(w, 72, 41); tnt(w, 98, 59); break;
  case 9:       /* a slim brick spire and a broad stone block, joined by a double-thick wooden bridge */
    HX(54, GR, 8, 6, 8, 2, 1, B, W_, B, 0, 0); HX(88, GR, 12, 4, 10, 1, 1, S, W_, S, 0, 0); rect_fill(w, 62, 27, 26, 2, W_);
    tgt(w, 58, 59); tgt(w, 58, 43); tgt(w, 58, 19); tgt(w, 75, 27); tgt(w, 93, 49); tgt(w, 94, 29); tnt(w, 76, 27); tnt(w, 58, 35); break;
  case 10: {    /* five wooden towers growing in width and height: topple the little one */
    static const int D[5][3] = {{48, 4, 2}, {55, 5, 3}, {63, 6, 4}, {72, 7, 5}, {82, 8, 6}};
    for(int k = 0; k < 5; k++) HX(D[k][0], GR, D[k][1], D[k][2], 8, 1, 1, W_, W_, W_, 0, 0);
    tgt(w, 49, 59); tgt(w, 57, 43); tgt(w, 66, 35); tgt(w, 75, 27); tgt(w, 85, 59); tgt(w, 85, 35); break; }
  case 11:      /* two big dunes under bouncy walls: nothing here wants to stay still */
    mound(w, 46, 38, 11); HX(56, 48, 16, 3, 7, 2, 1, RU, W_, W_, 1, 0);
    mound(w, 88, 26, 9);  HX(97, 50, 10, 3, 8, 2, 1, B, W_, W_, 0, 0);
    tgt(w, 60, 48); tgt(w, 66, 41); tgt(w, 60, 34); tgt(w, 101, 50); tgt(w, 101, 34); tgt(w, 103, 42);
    tnt(w, 66, 48); tnt(w, 103, 50); break;
  case 12:      /* a four-tier ziggurat with powder kegs buried in the two lower tiers */
    rect_fill(w, 50, 54, 44, 6, B); rect_fill(w, 56, 49, 32, 5, S); rect_fill(w, 62, 45, 20, 4, B); rect_fill(w, 68, 42, 8, 3, W_);
    rect_fill(w, 64, 56, 16, 3, MX_TNT); rect_fill(w, 68, 51, 8, 3, MX_TNT); tnt(w, 60, 49);
    tgt(w, 53, 54); tgt(w, 91, 54); tgt(w, 58, 49); tgt(w, 85, 49); tgt(w, 64, 45); tgt(w, 71, 42); break;
  case 13:      /* seven floors of 2-thick stone walls and steel slabs, next to a tall-roomed brick house */
    HX(72, GR, 16, 7, 7, 2, 2, S, ST, ST, 0, 0); HX(52, GR, 10, 3, 10, 1, 1, B, W_, W_, 1, 0);
    tgt(w, 76, 59); tgt(w, 84, 52); tgt(w, 76, 45); tgt(w, 84, 38); tgt(w, 76, 31); tgt(w, 84, 24); tgt(w, 56, 59); tgt(w, 58, 39);
    tnt(w, 80, 52); tnt(w, 80, 31); tnt(w, 58, 49); break;
  case 14:      /* fat stone piers, a 3-thick wooden deck; landlords on the deck and more in the brick hut under it */
    rect_fill(w, 56, 34, 8, 26, S); rect_fill(w, 98, 34, 8, 26, S); rect_fill(w, 56, 32, 50, 3, W_);
    HX(72, GR, 18, 2, 9, 2, 1, B, W_, W_, 0, 0);
    tgt(w, 66, 32); tgt(w, 80, 32); tgt(w, 94, 32); tgt(w, 77, 59); tgt(w, 85, 50); tnt(w, 88, 32); tnt(w, 62, 32); tnt(w, 80, 50); break;
  case 15:      /* a keep with 3-thick steel walls between a tall slim tower and a short fat one */
    HX(62, GR, 26, 2, 10, 3, 2, ST, S, ST, 0, 1); HX(50, GR, 10, 5, 8, 2, 2, S, S, S, 0, 1); HX(90, GR, 10, 4, 9, 2, 2, S, S, S, 0, 1);
    tgt(w, 68, 59); tgt(w, 78, 59); tgt(w, 72, 49); tgt(w, 80, 49); tgt(w, 55, 43); tgt(w, 55, 27); tgt(w, 95, 32);
    tnt(w, 72, 59); tnt(w, 76, 49); tnt(w, 94, 41); break;
  case 16:      /* long thin wooden slabs and short fat brick slabs on columns of different widths: pull the right one */
    for(int r = 0; r < 9; r++){
      int y = MX_ROWS - 1 - r * 5, th = (r & 1) ? 2 : 1;
      if(th == 1) rect_fill(w, 62, y, 30, 1, W_); else rect_fill(w, 66, y - 1, 22, 2, B);
      if(r < 8){
        int cw = (r & 1) ? 2 : 3, xa = (r & 1) ? 68 : 64, xb = (r & 1) ? 85 : 88, cm = (r & 1) ? W_ : B;
        rect_fill(w, xa, y - 4, cw, 4 - (th - 1), cm); rect_fill(w, xb, y - 4, cw, 4 - (th - 1), cm);
        if(r % 3 == 1) rect_fill(w, 76, y - 4, 2, 3, S);
      }
    }
    tgt(w, 70, 59); tgt(w, 80, 49); tgt(w, 72, 39); tgt(w, 84, 29); tgt(w, 76, 19); tnt(w, 78, 59); tnt(w, 70, 39); break;
  case 17:      /* a wide sand moat bank: knock the stone keep and the tall brick hall into the sand */
    mound(w, 44, 64, 10); HX(58, 49, 14, 3, 8, 2, 2, S, W_, S, 0, 1); HX(78, 49, 16, 2, 10, 1, 1, B, W_, W_, 1, 0);
    tgt(w, 62, 49); tgt(w, 67, 41); tgt(w, 62, 33); tgt(w, 82, 49); tgt(w, 88, 39); tgt(w, 75, 50); tnt(w, 66, 49); tnt(w, 86, 49); break;
  case 18:      /* a 40-wide glass skyscraper on a stone plinth, 2-thick wooden slabs */
    rect_fill(w, 56, 50, 48, 10, S); HX(60, 49, 40, 4, 8, 1, 2, MX_GLASS, W_, ST, 0, 0);
    tgt(w, 66, 49); tgt(w, 74, 49); tgt(w, 94, 49); tgt(w, 70, 41); tgt(w, 90, 33); tgt(w, 88, 25); tnt(w, 84, 49); tnt(w, 84, 41); break;
  case 19:      /* the finale: slim brick spire, wide stone hall, tall glass tower, two bridges */
    HX(46, GR, 8, 5, 8, 2, 1, B, W_, W_, 1, 0); HX(60, GR, 28, 4, 9, 2, 2, S, ST, S, 0, 1); HX(92, GR, 12, 6, 7, 1, 2, MX_GLASS, S, ST, 0, 0);
    rect_fill(w, 54, 41, 6, 1, W_); rect_fill(w, 88, 32, 4, 2, W_);
    tgt(w, 49, 59); tgt(w, 50, 43); tgt(w, 65, 59); tgt(w, 78, 59); tgt(w, 68, 50); tgt(w, 82, 41); tgt(w, 72, 32); tgt(w, 96, 52); tgt(w, 99, 38); tgt(w, 95, 24);
    tnt(w, 72, 50); tnt(w, 98, 59); tnt(w, 64, 41); break;
  /* chapter 2: the new mechanics that fit the consoles (pneumatic tubes, a sand flood, two pure-slingshot puzzles) */
  case 20:      /* a 3-storey steel vault nothing can plow into; three balloon-held pneumatic tubes post you inside */
    HX(62, GR, 34, 3, 12, 3, 3, ST, ST, ST, 0, 0);
    for(int f = 0; f < 3; f++){ int yb = 59 - f * 12; for(int y = yb - 9; y < yb; y++) set_cell(w, 79, y, 0); }   /* house_ex adds a centre partition when w > 13: the tube shot needs a clear room */
    { static const int FL[3] = {59, 47, 35}; for(int k = 0; k < 3; k++){ tgt(w, 71, FL[k]); tgt(w, 79, FL[k]); tgt(w, 87, FL[k]); tnt(w, 91, FL[k]); } }
    tube(w, R(37.5), R(42.5), R(68.25), R(57)); tube(w, R(51.25), R(29.5), R(68.25), R(45)); tube(w, R(41.25), R(19), R(68.25), R(33)); break;
  case 21:      /* a glass hourglass of sand over a steel bulb with three landlords in it: pop the wooden plug in the neck and the flood does the rest */
    hourglass(w, 80, 36, 14, 58, 14); rect_fill(w, 63, 59, 34, 1, ST); rect_fill(w, 63, 11, 34, 3, ST);
    rect_fill(w, 77, 32, 6, 7, W_); tnt(w, 79, 35); tnt(w, 79, 38);
    tgt(w, 72, 59); tgt(w, 80, 59); tgt(w, 88, 59);
    HX(44, GR, 12, 2, 8, 1, 1, B, W_, W_, 0, 0); tgt(w, 48, 59); tgt(w, 48, 51);
    HX(106, GR, 9, 4, 8, 1, 1, W_, W_, W_, 1, 0); tgt(w, 110, 59); tgt(w, 110, 35); break;
  case 22: {    /* walk-up wreckage: a 4-storey block you clear ON FOOT (the man lands on his feet), zig-zag: in at the front door, a pink pad at the far end of each floor boings you up its shaft */
    static const int PT[3] = {71, 83, 95}, PADS[3][2] = {{100, 0}, {60, 1}, {100, 2}};
    static const uint8_t DR[4][3] = {{0,1,0},{1,0,1},{0,1,1},{1,0,1}};      /* which partition doorways have a wooden door in them, per floor */
    static const int TG[11][2] = {{77,59},{89,59},{89,49},{77,49},{67,49},{67,39},{80,39},{90,39},{90,29},{77,29},{65,29}};
    const int g = 59, fh = 10, ft = 2, X = 58, Wd = 48, F = 4;
    w->walk = 1;
    rect_fill(w, X, g, Wd, 1, S);
    for(int f = 1; f <= F; f++) rect_fill(w, X - (f == F), g - f * fh, Wd + (f == F ? 2 : 0), ft, f == F ? B : W_);
    for(int k = 0; k < Wd + 2; k += 2) set_cell(w, X - 1 + k, g - F * fh - 1, B);
    for(int f = 0; f < F; f++){
      int yb = g - f * fh, y0 = yb - fh + ft, wm = f ? W_ : B;
      rect_fill(w, X, y0, 2, fh - ft, wm); rect_fill(w, X + Wd - 2, y0, 2, fh - ft, wm); room(w, X + 2, y0, Wd - 4, fh - ft, f);
      for(int pi = 0; pi < 3; pi++){ rect_fill(w, PT[pi], y0, 1, fh - ft, B); rect_fill(w, PT[pi], yb - 7, 1, 7, DR[f][pi] ? W_ : 0); }
      if(f == 0) rect_fill(w, X, y0, 2, 8, 0); else rect_fill(w, X, yb - 6, 2, 3, MX_GLASS);
      rect_fill(w, X + Wd - 2, yb - 6, 2, 3, MX_GLASS);
    }
    for(int k = 0; k < 3; k++){
      int c = PADS[k][0], f = PADS[k][1], yb = g - f * fh, ys = g - (f + 1) * fh;
      rect_fill(w, c, ys, 4, ft, 0); room(w, c, ys, 4, ft, f + 1);             /* the shaft through the slab above, and the pad under it */
      rect_fill(w, c, yb - 1, 4, 1, RU); pad(w, c, yb - 1, 4);
    }
    for(int k = 0; k < 11; k++) tgt(w, TG[k][0], TG[k][1]);
    tnt(w, 82, 59); tnt(w, 84, 49); tnt(w, 86, 39); tnt(w, 70, 29); break; }
  case 23:      /* a 27-tall steel wall with a stepped foot, and a wooden hut + tower behind it: only a high arc drops in */
    rect_fill(w, 50, 33, 5, 27, ST); rect_fill(w, 46, 48, 4, 12, ST); rect_fill(w, 43, 54, 3, 6, ST);
    HX(60, GR, 12, 2, 8, 1, 1, W_, W_, W_, 0, 0); HX(84, GR, 9, 4, 8, 1, 1, W_, W_, W_, 0, 0);
    tgt(w, 64, 59); tgt(w, 68, 59); tgt(w, 65, 51); tgt(w, 88, 59); tgt(w, 88, 43); tgt(w, 88, 35); tnt(w, 70, 59); tnt(w, 90, 43); break;
  case 24:      /* a full-height steel curtain with a 7-high slot per floor, and a wooden house behind with two landlords on each floor */
    rect_fill(w, 62, 8, 5, 52, ST); rect_fill(w, 58, 50, 4, 10, ST);
    HX(67, GR, 26, 3, 12, 1, 1, W_, W_, W_, 0, 0);
    for(int f = 0; f < 3; f++){ int yb = 59 - f * 12; rect_fill(w, 62, yb - 9, 5, 7, 0); tgt(w, 73, yb); tgt(w, 86, yb); tnt(w, 90, yb); } break;
  case 25:      /* collection day: a three-tier wooden scaffold, open at the sides, a walker pacing each half-deck */
    rect_fill(w, 62, 59, 37, 1, W_); rect_fill(w, 62, 47, 37, 2, W_); rect_fill(w, 62, 35, 37, 2, W_);
    for(int k = 0; k < 2; k++){ int y = k ? 37 : 49; rect_fill(w, 62, y, 1, k ? 12 : 10, W_); rect_fill(w, 80, y, 2, 12, W_); rect_fill(w, 98, y, 1, 12, W_); }
    rect_fill(w, 62, 24, 37, 2, W_); rect_fill(w, 62, 26, 1, 9, W_); rect_fill(w, 98, 26, 1, 9, W_);
    tgtk(w, 70, 59, LK_WALKER); tgtk(w, 90, 59, LK_WALKER); tgtk(w, 70, 47, LK_WALKER); tgtk(w, 90, 47, LK_WALKER); tgtk(w, 72, 35, LK_WALKER); tgt(w, 90, 35); tnt(w, 81, 59); break;
  case 26:      /* batter up: two bat landlords guard a wooden fort, two throwers inside lob briefcases at the sling through cut windows */
    HX(66, GR, 26, 3, 12, 1, 1, W_, W_, W_, 0, 0);
    rect_fill(w, 66, 38, 1, 7, 0); rect_fill(w, 66, 26, 1, 7, 0);
    tgtk(w, 61, 59, LK_BAT); tgtk(w, 64, 59, LK_BAT); tgt(w, 78, 59); tgtk(w, 72, 47, LK_THROWER); tgtk(w, 72, 35, LK_THROWER); tgt(w, 86, 35); tnt(w, 88, 47); tnt(w, 86, 59); break;
  case 27:      /* a six-floor wooden beanpole next to a brick cottage */
    house(w, 66, 8, 6, W_, W_, W_, 0, 0); house(w, 82, 12, 2, B, W_, W_, 1, 0);
    tgt(w, 70, 59); tgt(w, 70, 41); tgt(w, 70, 23); tgt(w, 70, 14); tgt(w, 86, 59); tgt(w, 90, 50); tnt(w, 72, 50); tnt(w, 88, 59); break;
  case 28:      /* three cottages with two towers of TNT between them */
    for(int k = 0; k < 3; k++) house(w, 50 + k * 18, 10, 2, B, W_, W_, 1, 0);
    rect_fill(w, 62, 52, 4, 8, MX_TNT); rect_fill(w, 80, 52, 4, 8, MX_TNT);
    tgt(w, 54, 59); tgt(w, 56, 50); tgt(w, 72, 59); tgt(w, 74, 50); tgt(w, 90, 59); tgt(w, 92, 50); tgt(w, 102, 60); break;
  case 29:      /* a big bouncy hall: come in through the wooden roof */
    house(w, 60, 30, 3, RU, W_, W_, 0, 0);
    tgt(w, 66, 59); tgt(w, 84, 59); tgt(w, 70, 50); tgt(w, 82, 50); tgt(w, 68, 41); tgt(w, 80, 41); tnt(w, 78, 59); tnt(w, 64, 50); tnt(w, 86, 41); break;
  case 30:      /* a landlord on every step */
    for(int k = 0; k < 6; k++){ rect_fill(w, 52 + k * 8, 60 - (k + 1) * 5, 8, (k + 1) * 5, (k & 1) ? B : W_); tgt(w, 56 + k * 8, 60 - (k + 1) * 5); if(k > 1) rect_fill(w, 55 + k * 8, 56, 2, 4, MX_TNT); }
    tnt(w, 102, 60); tnt(w, 48, 60); break;
  case 31:      /* a glass villa on top of a dune */
    mound(w, 50, 50, 14); house_at(w, 67, 45, 16, 3, MX_GLASS, W_, W_, 1, 0);
    tgt(w, 71, 45); tgt(w, 79, 45); tgt(w, 72, 36); tgt(w, 78, 27); tgt(w, 46, 60); tgt(w, 104, 60); tnt(w, 75, 45); break;
  case 32:      /* steel walls, wooden lid, fireworks inside */
    house(w, 64, 22, 2, ST, W_, W_, 0, 0); house(w, 92, 8, 1, W_, W_, W_, 1, 0);
    tgt(w, 68, 59); tgt(w, 82, 59); tgt(w, 72, 50); tgt(w, 80, 50); tgt(w, 96, 59); tnt(w, 70, 59); tnt(w, 80, 59); tnt(w, 77, 50); break;
  case 33:      /* three wooden decks between two stone pillars */
    rect_fill(w, 58, 24, 5, 36, S); rect_fill(w, 97, 24, 5, 36, S);
    rect_fill(w, 58, 46, 44, 2, W_); rect_fill(w, 58, 34, 44, 2, W_); rect_fill(w, 58, 22, 44, 2, W_);
    tgt(w, 70, 46); tgt(w, 88, 46); tgt(w, 74, 34); tgt(w, 84, 34); tgt(w, 80, 22); tgt(w, 66, 22); tgt(w, 80, 60);
    tnt(w, 79, 46); tnt(w, 92, 34); tnt(w, 70, 22); break;
  case 34:      /* four glass towers */
    for(int k = 0; k < 4; k++) house(w, 50 + k * 13, 9, 3 + (k & 1), MX_GLASS, MX_GLASS, MX_GLASS, 0, 0);
    tgt(w, 54, 59); tgt(w, 54, 41); tgt(w, 67, 50); tgt(w, 67, 32); tgt(w, 80, 59); tgt(w, 80, 41); tgt(w, 93, 50); tgt(w, 93, 32);
    tnt(w, 56, 50); tnt(w, 82, 50); break;
  case 35:      /* a brick penthouse on a steel tray on one stone column */
    rect_fill(w, 72, 30, 8, 30, S); rect_fill(w, 62, 28, 28, 2, ST);
    house_at(w, 64, 27, 24, 2, B, W_, W_, 1, 0); house(w, 96, 10, 1, W_, W_, W_, 0, 0);
    tgt(w, 69, 27); tgt(w, 84, 27); tgt(w, 72, 18); tgt(w, 82, 18); tgt(w, 100, 59); tgt(w, 54, 60); tnt(w, 78, 27); tnt(w, 79, 18); break;
  case 36:      /* the real finale: head office */
    house(w, 46, 10, 4, B, W_, W_, 1, 0); house(w, 60, 18, 5, S, ST, ST, 0, 1); house(w, 82, 12, 6, MX_GLASS, S, ST, 0, 0); house(w, 98, 10, 3, RU, W_, W_, 1, 0);
    tgt(w, 50, 59); tgt(w, 52, 32); tgt(w, 66, 59); tgt(w, 72, 50); tgt(w, 66, 41); tgt(w, 72, 32); tgt(w, 68, 23); tgt(w, 86, 59); tgt(w, 90, 41);
    tgt(w, 86, 23); tgt(w, 88, 14); tgt(w, 102, 59);
    tnt(w, 70, 59); tnt(w, 74, 41); tnt(w, 88, 50); tnt(w, 90, 32); tnt(w, 100, 50); tnt(w, 48, 50); break;
  default:      /* houses perched on sand: dig out the foundations and everything slumps */
    mound(w, 46, 30, 9); house_at(w, 56, 50, 10, 2, S, W_, S, 1, 0);
    mound(w, 86, 28, 9); house_at(w, 95, 50, 10, 2, B, W_, W_, 1, 0);
    tgt(w, 60, 50); tgt(w, 62, 41); tgt(w, 99, 50); tgt(w, 101, 41); tgt(w, 76, 59);
    tnt(w, 58, 59); tnt(w, 98, 59); break;
  }
}

void mx_load(MxWorld *w, int level){
  init_tables(); init_mats();
  int keep_sub = w->substeps, keep_wob = w->wob_enabled ? 1 : 0, first = !w->substeps;   /* front-end quality choices survive a reload */
  int keep_valve = w->valve_ok, keep_collapse = w->collapse, keep_nofight = w->nofight;
  memset(w, 0, sizeof *w);
  w->nofight = keep_nofight; w->rng = 0x2545F491u;
  w->dz_lo = MX_ROWS * MX_COLS; w->dz_hi = -1;
  w->bh_lo = 0; w->bh_hi = MX_ROWS + 3;               /* bhead starts zeroed, not -1: the first step clears all of it */
  w->valve_ok = keep_valve; w->collapse = keep_collapse;
  level %= MX_NUM_LEVELS;
  w->substeps = first ? SUBSTEPS : keep_sub;
  w->wob_enabled = first ? 1 : keep_wob;
  w->level = level; w->state = MX_PLAY; w->men = MEN[level]; w->banner = R(2.4);
  build(w, level);
  w->pw_left[PW_BEANS] = w->pw_left[PW_TOWEL] = w->pw_left[PW_SPLAT] = 1;
  w->sand_on = 1;
  /* anything that wouldn't hold at spawn is welded in place, so levels never collapse on load */
  support_pass(w);
  for(int c = 0; c < MX_MAX_CHUNKS; c++) if(w->chunks[c].active){ settle(w, &w->chunks[c], 1); }
  w->need_support = 0;
  next_man(w);
}

void mx_set_substeps(MxWorld *w, int n){ w->substeps = n < 2 ? 2 : n > 4 ? 4 : n; }
void mx_preview_point(const MxWorld *w, real t, real *ox, real *oy){
  const MxMan *m = &w->man;
  real vx = rmul(MX_ANCHOR_X - m->x, R(9.2)), vy = rmul(MX_ANCHOR_Y - m->y, R(9.2));
  *ox = m->x + rmul(vx, t);
  *oy = m->y + rmul(vy, t) + rmul(rmul(GRAV, R(0.5)), rmul(t, t));
}
int mx_targets_alive(const MxWorld *w){ int n = 0; for(int i = 0; i < w->ntargets; i++) n += w->targets[i].alive; return n; }
int mx_cells_left(const MxWorld *w){ int n = 0; for(int i = 0; i < MX_ROWS * MX_COLS; i++) n += w->grid[i] != 0; return n; }


/* ============================================== man jiggle
   Belly and rear hang off him on springs.  What moves them is his acceleration with gravity taken out (free fall
   does not make a belly sag): a launch stretches it back, a wall slams it forward, a landing squashes it.  Pure
   decoration - nothing in the physics reads these numbers. */
#define JIG_GAIN   R(0.45)
#define JIG_RGAIN  R(0.35)
#define JIG_MAX    R(1.5)
static void man_jiggle(MxWorld *w, real dt){
  MxMan *m = &w->man;
  if(dt <= 0) return;
  if(m->mode == MAN_NONE || m->mode == MAN_WAIT){
    m->bx = m->by = m->bvx = m->bvy = m->rx = m->ry = m->rvx = m->rvy = m->sq = 0;
    m->j_px = m->x; m->j_py = m->y; m->j_vx = m->j_vy = 0;
    return;
  }
  real vx = rdiv(m->x - m->j_px, dt), vy = rdiv(m->y - m->j_py, dt);
  real dvx = vx - m->j_vx, dvy = vy - m->j_vy - (m->mode == MAN_FLY ? rmul(GRAV, dt) : 0);   /* minus gravity: free fall is weightless */
  m->j_px = m->x; m->j_py = m->y; m->j_vx = vx; m->j_vy = vy;
  real mag = rabs(dvx) + rabs(dvy);
  if(mag < RI(150)){                                                       /* bigger than that is a teleport (respawn) */
    m->bvx -= rmul(dvx, JIG_GAIN);  m->bvy -= rmul(dvy, JIG_GAIN);
    m->rvx -= rmul(dvx, JIG_RGAIN); m->rvy -= rmul(dvy, JIG_RGAIN);
    if(mag > RI(20)){ m->sq = RI(1); m->sqx = dvx; m->sqy = dvy; }        /* hard hit: squash along the impact */
  }
  m->bvx += rmul(rmul(-RI(400), m->bx) - rmul(RI(7), m->bvx), dt); m->bvy += rmul(rmul(-RI(400), m->by) - rmul(RI(7), m->bvy), dt);
  m->rvx += rmul(rmul(-RI(250), m->rx) - rmul(RI(6), m->rvx), dt); m->rvy += rmul(rmul(-RI(250), m->ry) - rmul(RI(6), m->rvy), dt);
  m->bx = rclamp(m->bx + rmul(m->bvx, dt), -JIG_MAX, JIG_MAX); m->by = rclamp(m->by + rmul(m->bvy, dt), -JIG_MAX, JIG_MAX);
  m->rx = rclamp(m->rx + rmul(m->rvx, dt), -JIG_MAX, JIG_MAX); m->ry = rclamp(m->ry + rmul(m->rvy, dt), -JIG_MAX, JIG_MAX);
  m->sq = rmul(m->sq, R(0.88));
}

real mx_man_wobble(const MxMan *m){
  if(m->mode != MAN_STUCK) return 0;
  real x = rmul(m->t, R(3.2));                                   /* exp(-x) ~ 1 / (1 + x + x^2/2 + x^3/6) */
  real x2 = rmul(x, x), den = RI(1) + x + rmul(x2, R(0.5)) + rdiv(rmul(x2, x), RI(6));
  return rmul(rsin(rmul(m->t, RI(26))), rdiv(RI(1), den));
}

/* flight pose: pitch toward the path (eased like the browser's 15 % a frame), belly-flop spin */
static void man_pose(MxMan *m, real dt){
  if(m->mode != MAN_FLY){ m->ang = m->spin = 0; return; }
  if(m->dive){ m->spin += rmul(dt, RI(14)); return; }
  real tgt = rmul(ratan2(m->vy, rabs(m->vx)), R(0.6));
  m->ang += rmul(tgt - m->ang, rclamp(rmul(dt, RI(9)), 0, RI(1)));
}

/* ============================================================== update */
void mx_update(MxWorld *w, real dt){
  w->nev = 0; w->crack_ev = 0; w->time += dt;
  if(w->support_cd > 0) w->support_cd--;
  if(w->freeze > 0){ w->freeze--; return; }
  if(w->slow > 0){ w->slow -= dt; dt = rmul(dt, R(0.4)); }
  if(w->banner > 0) w->banner -= dt;
  if(w->flash > 0) w->flash -= dt;
  w->shake = rmul(w->shake, R(0.9));
  MxMan *m = &w->man;
  if(m->hurtCd > 0) m->hurtCd -= dt;
  if(m->stun > 0) m->stun -= dt;

  if(m->mode == MAN_AIM && (m->vlv || m->seated)){        /* on the air valve: fill up, and burst if he stays too long */
    if(m->seated){ m->x = MX_VALVE_X; m->y = MX_VALVE_Y; }
    m->inf += rdiv(dt, R(3.6)); m->airT += dt;
    if(m->airT > R(0.07)){ m->airT = 0; ev(w, EV_AIR, m->x, m->y, 0, m->inf, 0); }
    if(m->inf >= RI(1)){
      m->inf = RI(1); w->st_pops++; w->men--;
      ev(w, EV_AIRPOP, m->x, m->y, 0, 0, 0);
      if(w->shake < R(16)) w->shake = R(16);
      m->mode = MAN_WAIT; w->waitT = 0; m->vlv = m->seated = 0;
    }
  }

  if(m->mode == MAN_FLY){
    for(int i = 0; i < 8 && m->mode == MAN_FLY; i++){
#ifdef MX_FIXED
      real sub = (dt >> 3) + (i < (dt & 7));          /* 8 slices that add up to exactly dt (dt/8 alone loses ~6%) */
#else
      real sub = dt * 0.125f;
#endif
      man_step(w, sub);
    }
    if(m->mode == MAN_FLY){
      if(m->x > RI(MX_COLS + 12) || m->x < RI(-12) || m->gt > RI(9)) end_run(w);
    }
  } else if(m->mode == MAN_STUCK){
    m->t += dt; m->gooT += dt;
    int by = rfloor(m->y + MAN_R + R(0.25)), gx = rfloor(m->x);
    if(m->t < R(1.8) && m->y + MAN_R < RI(MX_ROWS) - R(0.25) && !(INB(gx, by) && w->grid[IDX(gx, by)]))
      m->y += rmul(R(2), dt);                                   /* sticky slide */
    if(m->gooT > R(0.35)){                                       /* goo eats nearby voxels */
      m->gooT = 0;
      int cx = rfloor(m->x), cy = rfloor(m->y);
      for(int j = -2; j <= 2; j++) for(int i = -2; i <= 2; i++) damage_cell(w, cx + i, cy + j, RI(5));
    }
    for(int i = 0; i < w->ntargets; i++){
      MxTarget *t = &w->targets[i];
      real dx = t->x - m->x, dy = t->y - m->y, r = MAN_R + R(0.875) + R(0.75);
      if(t->alive && rmul(dx, dx) + rmul(dy, dy) < rmul(r, r)) kill_target(w, t);
    }
    if(m->t > R(2.6)){ ev(w, EV_POP, m->x, m->y, 0, 0, 0); end_run(w); }
  } else if(m->mode == MAN_WALK){
    walk_step(w, dt);
  } else if(m->mode == MAN_WAIT){
    int nch = busy_count(w);
    w->waitT += dt;
    if(w->waitT > R(0.7) && (nch == 0 || w->waitT > R(3.5))) next_man(w);
  }

  if(w->retry_cd > 0 && --w->retry_cd == 0) w->need_support = 1;
  process_expl(w);
  if(w->need_support && w->support_cd <= 0){ support_pass(w); w->support_cd = MX_SUPPORT_INTERVAL; }
  int ns = w->substeps < 2 ? 2 : w->substeps > 4 ? 4 : w->substeps;
  for(int i = 0; i < ns; i++){
#ifdef MX_FIXED
    real sdt = dt / ns + (i < dt % ns);               /* slices add up to exactly dt */
#else
    real sdt = dt / (float)ns;
#endif
    step_chunks(w, sdt);
  }
  clean_chunks(w, dt);
  if(w->wob_on){
    wob_list(w);
    for(int i = 0; i < MX_WOB_STEPS; i++) wobble_step(w, dt / MX_WOB_STEPS);
    wobble_check(w);
  }
  w->sand_t++;
  if(w->sand_on && (w->sand_t % 3) == 0) sand_step(w);
  if(w->need_support && w->support_cd <= 0){ support_pass(w); w->support_cd = MX_SUPPORT_INTERVAL; }
  upd_targets(w, dt);
  upd_cases(w, dt);
  man_jiggle(w, dt);
  man_pose(m, dt);

  if(w->state == MX_PLAY){
    int nch = busy_count(w);
    if(mx_targets_alive(w) == 0){
      w->winT += dt;
      if(w->winT > R(1.6)){ w->state = MX_WIN; w->score += w->men * 3000; ev(w, EV_WIN, 0, 0, 0, 0, 0); }
    } else if(w->men == 0 && m->mode == MAN_NONE){
      if(nch == 0){
        w->loseT += dt;
        if(w->loseT > R(1.8)){ w->state = MX_LOSE; ev(w, EV_LOSE, 0, 0, 0, 0, 0); }
      } else {
        /* safety net: never let a stubborn chunk block the end of the level.  After 8 s put it back into the grid;
           if it keeps coming loose again (settled mid-air, support check detaches it next frame: seen on Level 6
           with the rubber blocks) throw it away after 10 s so the level can always end. */
        w->waitT += dt;
        if(w->waitT > RI(8)) for(int c = 0; c < MX_MAX_CHUNKS; c++) if(w->chunks[c].active){
          if(w->waitT > RI(10)) w->chunks[c].active = 0; else settle(w, &w->chunks[c], 0);
        }
      }
    }
  }
}

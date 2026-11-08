#include "mx.h"
#include <string.h>
#include <math.h>

#define IDX(x, y) ((y) * MX_COLS + (x))
#define INB(x, y) ((x) >= 0 && (x) < MX_COLS && (y) >= 0 && (y) < MX_ROWS)

/* ---- tuning (cell units; 1 cell = 8 px in the HTML prototype) ---- */
#define GRAV      R(112.5)
#define KCOST     R(17.19)      /* energy cost per hp */
#define RC        R(0.5)        /* chunk cell radius */
#define MAN_R     R(1.75)
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
static void wob_zero(MxWorld *w, int i){ w->ox[i] = w->oy[i] = w->ovx[i] = w->ovy[i] = 0; }

static real kill(MxWorld *w, int x, int y, real bvx, real bvy){
  int i = IDX(x, y), m = w->grid[i];
  if(!m) return 0;
  real h = w->hp[i];
  w->grid[i] = 0; w->hp[i] = 0; w->anch[i] = 0; w->need_support = 1; w->score += 10;
  wob_zero(w, i); clear_links(w, i); w->sand_on = 1;
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

static void apply_impulse(MxChunk *ch, real px, real py, real jx, real jy){
  ch->vx += rdiv(jx, ch->m); ch->vy += rdiv(jy, ch->m);
  ch->w  += rdiv(rmul(px - ch->x, jy) - rmul(py - ch->y, jx), ch->I);
  ch->sleep = 0;
}

static void kill_target(MxWorld *w, MxTarget *t){
  if(!t->alive) return;
  t->alive = 0; w->score += 5000; w->slow = R(0.25);
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
static int wob_nact;
static real KK[MX_NMAT][MX_NMAT];                       /* joint stiffness of every pair of kinds (the weaker one) */
static void wob_list(const MxWorld *w){
  int n = 0;
  for(int i = 0; i < (MX_ROWS - 1) * MX_COLS; i++) if(w->grid[i]) wob_act[n++] = (uint16_t)i;
  wob_nact = n;
}
static MX_HOT void wobble_step(MxWorld *w, real dt){
  real fd[MX_NMAT];
  for(int m = 0; m < MX_NMAT; m++){ fd[m] = RI(1) - rmul(MATS[m].damp, dt); if(fd[m] < 0) fd[m] = 0; }
  if(KK[1][1] == 0) for(int a = 1; a < MX_NMAT; a++) for(int b = 1; b < MX_NMAT; b++) KK[a][b] = MATS[a].K < MATS[b].K ? MATS[a].K : MATS[b].K;
  /* pass 1: forces -> velocities (reads only positions, so the order does not matter) */
  for(int n = 0; n < wob_nact; n++){
    int i = wob_act[n], x = i % MX_COLS, y = i / MX_COLS, m = w->grid[i];
    real lean = rmul(LEANK, RI(MX_ROWS - 1 - y)) - HOMEK;
    uint8_t bk = w->brk[i];
    real ax = 0, ay = 0, oxi = w->ox[i], oyi = w->oy[i];
    const real *Km = KK[m];
    int nj[4] = { (x + 1 < MX_COLS && !(bk & 1)) ? i + 1 : -1, (x > 0 && !(bk & 2)) ? i - 1 : -1,
                  !(bk & 4) ? i + MX_COLS : -1, (y > 0 && !(bk & 8)) ? i - MX_COLS : -1 };
    for(int d = 0; d < 4; d++){                         /* every neighbour pulls in both axes (shear carries the sway up) */
      int j = nj[d], mj;
      if(j < 0 || !(mj = w->grid[j])) continue;
      real K = Km[mj];
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
    int i = wob_act[n], x = i % MX_COLS, m = w->grid[i];
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

static MxChunk *chunk_slot(MxWorld *w){
  for(int c = 0; c < MX_MAX_CHUNKS; c++) if(!w->chunks[c].active) return &w->chunks[c];
  return 0;
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
  memset(w->bhead, 0xff, sizeof w->bhead);
  for(int c = 0; c < MX_MAX_CHUNKS; c++){
    MxChunk *ch = &w->chunks[c];
    if(!ch->active) continue;
    ch->vy += rmul(GRAV, dt);
    ch->vx = rmul(ch->vx, R(0.9995)); ch->w = rmul(ch->w, R(0.998));
    ch->x += rmul(ch->vx, dt); ch->y += rmul(ch->vy, dt); ch->ang += rmul(ch->w, dt);
    ch->cs = rcos(ch->ang); ch->sn = rsin(ch->ang); ch->cx = ch->cy = 0; ch->cn = 0;
    for(int i = 0; i < ch->count; i++){
      MxCell *a = &ch->cells[i];
      a->wx = ch->x + rmul(ch->cs, a->lx) - rmul(ch->sn, a->ly);
      a->wy = ch->y + rmul(ch->sn, a->lx) + rmul(ch->cs, a->ly);
      if(!a->edge) continue;
      int gx = rfloor(a->wx) + 2, gy = rfloor(a->wy) + 2;
      if(gx < 0) gx = 0;
      if(gx > MX_COLS + 3) gx = MX_COLS + 3;
      if(gy < 0) gy = 0;
      if(gy > MX_ROWS + 3) gy = MX_ROWS + 3;
      int b = gy * (MX_COLS + 4) + gx, id = c * MX_CHUNK_CELLS + i;
      w->bnext[id] = w->bhead[b]; w->bhead[b] = (int16_t)id;
    }
  }
  for(int c = 0; c < MX_MAX_CHUNKS; c++){
    MxChunk *ch = &w->chunks[c];
    if(!ch->active) continue;
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
          if(oc <= c) continue;
          MxChunk *och = &w->chunks[oc];
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
            real den = rdiv(RI(1), ch->m) + rdiv(RI(1), och->m) + rdiv(rmul(rna, rna), ch->I) + rdiv(rmul(rnb, rnb), och->I);
            real jj = rdiv(rmul(-vn, R(1.1)), den);
            ch->vx += rdiv(rmul(jj, nx), ch->m); ch->vy += rdiv(rmul(jj, ny), ch->m); ch->w += rdiv(rmul(rna, jj), ch->I);
            och->vx -= rdiv(rmul(jj, nx), och->m); och->vy -= rdiv(rmul(jj, ny), och->m); och->w -= rdiv(rmul(rnb, jj), och->I);
            if(-vn > R(25)){
              real dm = rmul(-vn - R(22.5), R(0.04));
              a->h -= dm; o->h -= dm;
              if(a->h <= 0) a->dead = 1;
              if(o->h <= 0) o->dead = 1;
            }
          }
          real tm = ch->m + och->m;
          real fa = rmul(R(0.5), rmul(pen, rdiv(och->m, tm))), fb = rmul(R(0.5), rmul(pen, rdiv(ch->m, tm)));
          ch->cx += rmul(nx, fa); ch->cy += rmul(ny, fa); ch->cn++;
          och->cx -= rmul(nx, fb); och->cy -= rmul(ny, fb); och->cn++;
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
        int j = IDX(X, Y); w->grid[j] = a->m; w->hp[j] = a->h; w->anch[j] = 1; ok = 1;
        wob_zero(w, j); clear_links(w, j); if(a->m == MX_SAND) w->sand_on = 1;
      }
    }
  }
  ch->active = 0;
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
      if(ch->sleep > R(0.5)){ settle(w, ch, 0); continue; }
    } else ch->sleep = 0;
    /* second test over a whole second: a resting stack can jitter by +-0.1 cell every frame (never
       "slow" frame to frame, e.g. in fixed point with few substeps) yet go nowhere */
    if(ch->wt <= 0){ ch->rx = ch->x; ch->ry = ch->y; ch->ra = ch->ang; }
    ch->wt += dt;
    if(ch->wt >= RI(1)){
      real mv = rabs(ch->x - ch->rx) + rabs(ch->y - ch->ry) + rmul(rabs(ch->ang - ch->ra), R(2.25));
      ch->wt = 0;
      if(mv < R(0.75) && ch->age > RI(1)){ settle(w, ch, 0); continue; }
    }
  }
}

/* ============================================================ the man */
static void stick(MxWorld *w){
  MxMan *m = &w->man;
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
  real mass = m->dive ? R(2.6) : RI(1);
  real s2 = rmul(m->vx, m->vx) + rmul(m->vy, m->vy);
  real E = rmul(R(0.5), rmul(mass, s2)), E0 = E;
  int touched = 0;
  int x0 = rfloor(m->x - MAN_R), x1 = rfloor(m->x + MAN_R), y0 = rfloor(m->y - MAN_R), y1 = rfloor(m->y + MAN_R);
  struct { real d2; int x, y; } list[25];
  int n = 0;
  real rr = rmul(MAN_R, MAN_R);
  for(int y = y0; y <= y1; y++) for(int x = x0; x <= x1; x++){
    if(!INB(x, y) || !w->grid[IDX(x, y)] || w->grid[IDX(x, y)] == MX_RUBBER) continue;
    real qx = rclamp(m->x, RI(x), RI(x + 1)), qy = rclamp(m->y, RI(y), RI(y + 1));
    real dx = m->x - qx, dy = m->y - qy, d2 = rmul(dx, dx) + rmul(dy, dy);
    if(d2 >= rr || n >= 25) continue;
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
    if(rmul(dx, dx) + rmul(dy, dy) < rmul(r, r)) kill_target(w, t);
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
  m->vy += rmul(GRAV, dt); m->x += rmul(m->vx, dt); m->y += rmul(m->vy, dt); m->gt += dt;
  if(m->hitCd > 0) m->hitCd -= dt;
  if(m->y + MAN_R >= RI(MX_ROWS)){
    m->y = RI(MX_ROWS) - MAN_R;
    stick(w);
    return;
  }
  man_plow(w);
}

static void next_man(MxWorld *w){
  memset(&w->man, 0, sizeof w->man);
  if(w->men > 0){ w->man.mode = MAN_AIM; w->man.x = MX_ANCHOR_X; w->man.y = MX_ANCHOR_Y; }
  else w->man.mode = MAN_NONE;
}

void mx_set_pull(MxWorld *w, real dx, real dy){
  MxMan *m = &w->man;
  if(m->mode != MAN_AIM) return;
  real d = rsqrt(rmul(dx, dx) + rmul(dy, dy));
  if(d > MX_PULL_MAX){ real k = rdiv(MX_PULL_MAX, d); dx = rmul(dx, k); dy = rmul(dy, k); }
  m->x = MX_ANCHOR_X + dx; m->y = MX_ANCHOR_Y + dy;
}
void mx_release(MxWorld *w){
  MxMan *m = &w->man;
  if(m->mode != MAN_AIM || w->state != MX_PLAY) return;
  real dx = MX_ANCHOR_X - m->x, dy = MX_ANCHOR_Y - m->y;
  if(rmul(dx, dx) + rmul(dy, dy) < R(3)){ m->x = MX_ANCHOR_X; m->y = MX_ANCHOR_Y; return; }
  m->vx = rmul(dx, R(9.2)); m->vy = rmul(dy, R(9.2));
  m->mode = MAN_FLY; m->gt = 0; w->men--;
  ev(w, EV_LAUNCH, m->x, m->y, 0, 0, 0);
}
void mx_dive(MxWorld *w){
  MxMan *m = &w->man;
  if(m->mode != MAN_FLY || m->dive) return;
  m->dive = 1; m->vx = rmul(m->vx, R(0.4));
  if(m->vy < 0) m->vy = 0;
  m->vy += R(97.5);
  if(w->shake < R(3)) w->shake = R(3);
  ev(w, EV_DIVE, m->x, m->y, 0, 0, 0);
}

/* ========================================================== landlords */
static void upd_targets(MxWorld *w, real dt){
  for(int i = 0; i < w->ntargets; i++){
    MxTarget *t = &w->targets[i];
    if(!t->alive) continue;
    t->t += dt;
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
static void house_at(MxWorld *w, int x, int g, int ww, int floors, int wall, int floor, int roof, int gable, int crenel){
  const int fh = 9, top = g + 1 - (floors * fh + 1);
  for(int f = 0; f <= floors; f++) rect_fill(w, x, g - f * fh, ww, 1, floor);
  for(int f = 0; f < floors; f++){
    int yb = g - f * fh;
    if(w->nrooms < MX_MAX_ROOMS){
      MxRoom *rm = &w->rooms[w->nrooms++];
      rm->x = (uint8_t)(x + 1); rm->y = (uint8_t)(yb - fh + 1); rm->w = (uint8_t)(ww - 2); rm->h = (uint8_t)(fh - 1); rm->pal = (uint8_t)(w->nhouses & 3);
    }
    for(int y = yb - fh + 1; y < yb; y++){
      set_cell(w, x, y, wall); set_cell(w, x + ww - 1, y, wall);
      if(ww > 13) set_cell(w, x + (ww >> 1), y, wall == MX_STEEL ? MX_STEEL : MX_WOOD);
    }
    for(int y = yb - 5; y < yb - 3; y++){ set_cell(w, x, y, MX_GLASS); set_cell(w, x + ww - 1, y, MX_GLASS); }
  }
  if(gable){ for(int k = 0; k < 7 && ww + 2 - 2 * k > 1; k++) rect_fill(w, x - 1 + k, top - 1 - k, ww + 2 - 2 * k, 1, roof); }
  else rect_fill(w, x - 1, top - 2, ww + 2, 2, roof);
  if(crenel) for(int k = 0; k < ww + 2; k += 2) set_cell(w, x - 1 + k, top - 3, roof);
  w->nhouses++;
}
static void house(MxWorld *w, int x, int ww, int floors, int wall, int floor, int roof, int gable, int crenel){
  house_at(w, x, MX_ROWS - 1, ww, floors, wall, floor, roof, gable, crenel);
}
/* a truncated sand pyramid (45 degree slopes are what sand can hold) */
static void mound(MxWorld *w, int x, int width, int height){
  for(int r = 0; r < height && width - 2 * r > 0; r++) rect_fill(w, x + r, MX_ROWS - 1 - r, width - 2 * r, 1, MX_SAND);
}
static void tnt(MxWorld *w, int x, int y){ rect_fill(w, x, y - 2, 2, 2, MX_TNT); }
static void tgt(MxWorld *w, int cx, int row){
  if(w->ntargets >= MX_MAX_TARGETS) return;
  MxTarget *t = &w->targets[w->ntargets++];
  t->x = RI(cx) + R(0.5); t->y = RI(row) - R(0.875); t->vy = 0; t->t = 0; t->alive = 1;
}

static const char *NAMES[MX_NUM_LEVELS] = {
  "Suburban Eviction", "Duplex Dilemma", "Tower Block Tussle", "Castle Foreclosure", "Corporate Takeover",
  "Bouncy Castle", "Sandcastle Shakedown"};
static const int MEN[MX_NUM_LEVELS] = {4, 5, 6, 6, 7, 6, 7};
const char *mx_level_name(int l){ return NAMES[l % MX_NUM_LEVELS]; }

static void build(MxWorld *w, int l){
  const int W_ = MX_WOOD, B = MX_BRICK, S = MX_STONE, ST = MX_STEEL, RU = MX_RUBBER;
  switch(l){
  case 0:
    house(w, 76, 16, 2, B, W_, W_, 1, 0); tgt(w, 81, 59); tgt(w, 90, 50); tnt(w, 86, 59); break;
  case 1:
    house(w, 58, 12, 2, W_, W_, W_, 1, 0); house(w, 80, 16, 3, B, W_, B, 1, 0);
    tgt(w, 63, 59); tgt(w, 66, 50); tgt(w, 84, 59); tgt(w, 90, 50); tgt(w, 86, 41); tnt(w, 92, 59); tnt(w, 62, 50); break;
  case 2:
    house(w, 62, 10, 2, W_, W_, W_, 1, 0); house(w, 82, 14, 6, B, S, S, 0, 0);
    tgt(w, 66, 59); tgt(w, 67, 50); tgt(w, 87, 59); tgt(w, 93, 41); tgt(w, 86, 23); tnt(w, 85, 32); tnt(w, 92, 50); break;
  case 3:
    house(w, 52, 8, 4, S, S, S, 1, 0); house(w, 64, 26, 3, S, S, S, 0, 1); house(w, 92, 8, 4, S, S, S, 1, 0);
    tgt(w, 55, 41); tgt(w, 68, 59); tgt(w, 73, 50); tgt(w, 83, 59); tgt(w, 95, 32); tnt(w, 84, 59); tnt(w, 70, 41); break;
  case 4:
    house(w, 54, 12, 3, B, W_, B, 1, 0); house(w, 76, 20, 5, ST, S, ST, 0, 0);
    tgt(w, 59, 59); tgt(w, 62, 50); tgt(w, 80, 59); tgt(w, 90, 50); tgt(w, 84, 41); tgt(w, 92, 32); tgt(w, 82, 23);
    tnt(w, 88, 59); tnt(w, 80, 32); tnt(w, 92, 23); break;
  case 5:       /* rubber walls: the man rebounds, so he has to come in through the wooden roofs or use TNT */
    house(w, 54, 14, 3, RU, W_, W_, 1, 0); house(w, 78, 14, 4, RU, B, W_, 0, 0);
    tgt(w, 59, 59); tgt(w, 63, 50); tgt(w, 59, 41); tgt(w, 83, 59); tgt(w, 87, 50); tgt(w, 83, 41); tgt(w, 87, 32);
    tnt(w, 64, 59); tnt(w, 88, 41); break;
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
  memset(w, 0, sizeof *w);
  level %= MX_NUM_LEVELS;
  w->substeps = first ? SUBSTEPS : keep_sub;
  w->wob_enabled = first ? 1 : keep_wob;
  w->level = level; w->state = MX_PLAY; w->men = MEN[level]; w->banner = R(2.4);
  build(w, level);
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
      if(m->x > RI(MX_COLS + 12) || m->x < RI(-12) || m->gt > RI(9)) m->mode = MAN_WAIT, w->waitT = 0;
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
    if(m->t > R(2.6)){ ev(w, EV_POP, m->x, m->y, 0, 0, 0); m->mode = MAN_WAIT; w->waitT = 0; }
  } else if(m->mode == MAN_WAIT){
    int nch = 0;
    for(int c = 0; c < MX_MAX_CHUNKS; c++) nch += w->chunks[c].active;
    w->waitT += dt;
    if(w->waitT > R(0.7) && (nch == 0 || w->waitT > R(3.5))) next_man(w);
  }

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
  man_jiggle(w, dt);

  if(w->state == MX_PLAY){
    int nch = 0;
    for(int c = 0; c < MX_MAX_CHUNKS; c++) nch += w->chunks[c].active;
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

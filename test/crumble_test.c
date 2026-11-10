/* Post-run crumble (core aftershock).  White-box: this file #includes core/mx.c so it can call the static aftershock() directly.
   Checks the exact rules (70% health line, weakest 80 first, joints snapped on both sides, sand left alone, only where something was damaged)
   and the gameplay path (it fires once when a run really ends, never while he is flying, and a blasted wood tower comes apart within two
   seconds of the run ending).  Float and fixed point.
     gcc -std=c99 -O2 -I../core crumble_test.c -lm            (and again with -DMX_FIXED) */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../core/mx.c"
#ifdef MX_FIXED
#define RS 4096.0
#else
#define RS 1.0
#endif
static int fails;
#define CHECK(c, msg) do{ if(!(c)){ printf("FAIL: %s\n", msg); fails++; } else printf("ok: %s\n", msg); }while(0)
static MxWorld W;
#define DT R(1.0 / 60.0)
#define IX(x, y) ((y) * MX_COLS + (x))

/* an empty world (one landlord parked far away so the level cannot be won) */
static void empty(void){
  mx_load(&W, 0);
  memset(W.grid, 0, sizeof W.grid); memset(W.hp, 0, sizeof W.hp); memset(W.anch, 0, sizeof W.anch); memset(W.brk, 0, sizeof W.brk); memset(W.dmg, 0, sizeof W.dmg);
  W.dz_lo = MX_ROWS * MX_COLS; W.dz_hi = -1; W.need_support = 0; W.nrooms = 0;
  W.ntargets = 1; W.targets[0].x = RI(5); W.targets[0].y = RI(40); W.targets[0].vy = 0; W.targets[0].t = 0; W.targets[0].alive = 1;
}
static void block(int x0, int y0, int w, int h, int m){ for(int y = y0; y < y0 + h; y++) for(int x = x0; x < x0 + w; x++){ W.grid[IX(x, y)] = (uint8_t)m; W.hp[IX(x, y)] = RI(mx_material_hp(m)); } }
static void flag(int i){ W.dmg[i] = 1; dz_touch(&W, i); }
static void flag_box(int x0, int y0, int x1, int y1){ for(int y = y0; y <= y1; y++) for(int x = x0; x <= x1; x++) flag(IX(x, y)); }
static int count_chunks(void){ int n = 0; for(int c = 0; c < MX_MAX_CHUNKS; c++) n += W.chunks[c].active; return n; }
static void fly(real x, real y, real vx, real vy){ W.man.mode = MAN_FLY; W.man.x = x; W.man.y = y; W.man.vx = vx; W.man.vy = vy; W.man.rad = MAN_R0; W.man.gt = 0; if(W.men < 1) W.men = 1; }

int main(void){
  real cx, cy;

  /* ---- the exact rules, by calling aftershock() on a hand-made wall ---- */
  empty(); block(40, 40, 20, 19, MX_BRICK); block(30, 59, 40, 1, MX_BRICK);               /* brick hp is 40 */
  W.hp[IX(45, 45)] = RI(5); W.hp[IX(46, 45)] = RI(9); W.hp[IX(47, 45)] = RI(12);          /* 12%, 22%, 30%: weak */
  W.hp[IX(44, 45)] = RI(35);                                                              /* 87%: not weak */
  W.hp[IX(52, 50)] = RI(27);                                                              /* 67.5%: weak, but nothing is damaged anywhere near it */
  flag_box(44, 44, 48, 46);
  { int n = aftershock(&W, &cx, &cy);
    CHECK(n == 3, "three blocks below 70% inside the damaged zone crumble; the healthy one and the far one do not");
    CHECK(W.brk[IX(45, 45)] == 15 && W.brk[IX(46, 45)] == 15 && W.brk[IX(47, 45)] == 15, "each weak block has all four joints snapped");
    CHECK((W.brk[IX(44, 45)] & 1) && !(W.brk[IX(44, 45)] & 14) && (W.brk[IX(48, 45)] & 2) && !(W.brk[IX(48, 45)] & 13), "...and the joint is snapped on the neighbour's side too (and only that one)");
    CHECK(W.brk[IX(52, 50)] == 0, "a weak block with no damage near it is left alone");
    CHECK(fabs((double)cx / RS - 46.5) < 0.01 && fabs((double)cy / RS - 45.5) < 0.01, "the reported centre is the middle of the cracked blocks");
    CHECK(W.need_support == 1 && W.dz_hi < W.dz_lo, "the support pass is forced and the damage list is empty again");
    CHECK(aftershock(&W, &cx, &cy) == 0, "a second call with nothing new damaged does nothing"); }

  /* the boundary of the health line: 69% crumbles, 70% does not (wood hp 20) */
  empty(); block(40, 52, 20, 7, MX_WOOD); W.hp[IX(50, 55)] = RI(13) + RI(1) / 5 * 4 - RI(1) / 5 * 4; W.hp[IX(52, 55)] = RI(14);   /* 13/20 = 65%, 14/20 = 70% exactly */
  flag_box(49, 54, 53, 56);
  CHECK(aftershock(&W, &cx, &cy) == 1 && W.brk[IX(50, 55)] == 15 && W.brk[IX(52, 55)] != 15, "65% crumbles, exactly 70% does not (hp * 10 < full * 7)");

  /* the cap and the order: 200 weak blocks spaced three apart (so no two touch), each at a different health: only the weakest 80 snap */
  empty(); block(20, 20, 78, 39, MX_WOOD);
  { int nweak = 0, hpv[2400], at[2400];
    for(int y = 21; y < 58; y += 3) for(int x = 21; x < 97; x += 3){ int v = 1 + (nweak * 7) % 13;  /* 1..13 of 20 = 5%..65%, evenly mixed */
      W.hp[IX(x, y)] = RI(v); hpv[nweak] = v; at[nweak] = IX(x, y); nweak++; }
    for(int i = 0; i < MX_ROWS * MX_COLS; i++) if(W.grid[i]) flag(i);
    int n = aftershock(&W, &cx, &cy), sel_max = 0, unsel_min = 99, sel = 0;
    for(int k = 0; k < nweak; k++){ if(W.brk[at[k]] == 15){ sel++; if(hpv[k] > sel_max) sel_max = hpv[k]; } else if(hpv[k] < unsel_min) unsel_min = hpv[k]; }
    printf("  %d weak blocks, %d snapped; healthiest snapped = %d/20, weakest left alone = %d/20\n", nweak, sel, sel_max, unsel_min);
    CHECK(nweak > 150 && n == 80 && sel == 80, "with more than 80 weak blocks exactly 80 crumble");
    CHECK(sel_max <= unsel_min, "...and they are the weakest ones (nothing left alone is weaker than anything that crumbled)"); }

  /* sand is never chosen, never gets a snapped joint, and keeps its anchor flag; the wood next to it still crumbles */
  empty(); block(40, 50, 10, 9, MX_SAND); block(50, 50, 10, 9, MX_WOOD);
  for(int y = 50; y < 59; y++) for(int x = 40; x < 50; x++){ W.hp[IX(x, y)] = RI(1); W.anch[IX(x, y)] = 1; }
  W.hp[IX(51, 54)] = RI(2); flag_box(38, 50, 55, 58);
  { int n = aftershock(&W, &cx, &cy), sandbrk = 0, sandanch = 0;
    for(int y = 50; y < 59; y++) for(int x = 40; x < 50; x++){ sandbrk += W.brk[IX(x, y)] != 0; sandanch += W.anch[IX(x, y)]; }
    CHECK(n == 1 && W.brk[IX(51, 54)] == 15, "very weak sand next to damaged wood: only the wood block crumbles");
    CHECK(sandbrk == 0 || sandbrk == 0, "sand blocks never get snapped joints");
    CHECK(sandanch == 90, "...and the sand keeps its anchor flag (it has no weld to lose)"); }

  /* the damage list only covers rows that matter: damage at the very top and the very bottom of the map */
  empty(); block(10, 0, 5, 3, MX_WOOD); block(100, 57, 5, 2, MX_WOOD);
  for(int y = 0; y < 3; y++) for(int x = 10; x < 15; x++) W.hp[IX(x, y)] = RI(2);
  for(int y = 57; y < 59; y++) for(int x = 100; x < 105; x++) W.hp[IX(x, y)] = RI(2);
  flag(IX(12, 1)); flag(IX(102, 58));
  CHECK(aftershock(&W, &cx, &cy) > 0 && W.brk[IX(12, 1)] == 15 && W.brk[IX(102, 57)] != 0, "damage on the first and the last rows is handled (no scan outside the map)");

  /* ---- gameplay path ---- */
  /* a shot that goes over everything: the run ends (he leaves the map) and nothing crumbles */
  empty(); block(66, 24, 6, 36, MX_WOOD);
  { for(int i = 0; i < 100; i++) mx_update(&W, DT);
    int c0 = mx_cells_left(&W), crumbles = 0, ended = 0;
    W.men = 3; fly(RI(20), R(1), RI(250), -RI(20));
    for(int f = 0; f < 60 * 12; f++){ mx_update(&W, DT); for(int i = 0; i < W.nev; i++) crumbles += W.ev[i].type == EV_CRUMBLE; if(W.man.mode == MAN_WAIT) ended = 1; }
    CHECK(ended && crumbles == 0 && mx_cells_left(&W) == c0 && count_chunks() == 0, "a run that hits nothing ends quietly: no crumble, the tower stands for ten seconds"); }

  /* a wood tower with a TNT charge in it: the blast leaves a ring of half-dead blocks.  Fire several angles; every shot that sets the TNT off must
     crumble exactly once, only after the run has ended, with at most 80 blocks, and the tower must come apart within two seconds of that */
  { int booms = 0, bad_once = 0, bad_flying = 0, bad_cap = 0, apart = 0;
    for(int deg = 14; deg <= 50; deg += 3){
      empty(); block(66, 24, 8, 36, MX_WOOD); block(69, 44, 2, 2, MX_TNT);
      for(int i = 0; i < 100; i++) mx_update(&W, DT);
      double a = deg * 3.14159265 / 180.0;
      mx_set_pull(&W, (real)(-11.5 * cos(a) * RS), (real)(11.5 * sin(a) * RS)); mx_release(&W);
      int boom = 0, crumbles = 0, nblocks = 0, end_f = -1, flying_c = 0, destroyed_after = 0, chunks_after = 0, cells_end = 0;
      for(int f = 0; f < 60 * 12; f++){
        int flying = W.man.mode == MAN_FLY || W.man.mode == MAN_STUCK;
        mx_update(&W, DT);
        for(int i = 0; i < W.nev; i++){
          if(W.ev[i].type == EV_BOOM) boom = 1;
          if(W.ev[i].type == EV_CRUMBLE){ crumbles++; nblocks = (int)(W.ev[i].a / RS); if(flying && W.man.mode != MAN_WAIT) flying_c++; }
        }
        if(crumbles == 1 && end_f < 0){ end_f = f; cells_end = mx_cells_left(&W); }
        if(end_f >= 0 && f <= end_f + 120){ if(count_chunks() > chunks_after) chunks_after = count_chunks(); }
        if(end_f >= 0 && f == end_f + 120) destroyed_after = cells_end - mx_cells_left(&W);
      }
      if(!boom) continue;
      booms++;
      if(crumbles != 1) bad_once++;
      if(flying_c) bad_flying++;
      if(nblocks > 80) bad_cap++;
      if(chunks_after > 0 || destroyed_after > 0) apart++;
      printf("  TNT in a wood tower, %2d deg: crumbles=%d (%d blocks) loose chunks within 2 s=%d blocks lost within 2 s=%d\n", deg, crumbles, nblocks, chunks_after, destroyed_after);
    }
    CHECK(booms >= 3, "at least three of the angles set the TNT off");
    CHECK(bad_once == 0, "every blast run crumbles exactly once");
    CHECK(bad_flying == 0, "...only after the run has ended, never while he is still in the air");
    CHECK(bad_cap == 0, "...with at most 80 blocks");
    CHECK(apart == booms, "...and the tower comes apart within two seconds of the run ending (loose chunks or blocks lost)"); }

  /* bursting on the air valve is not a run: no aftershock */
  empty(); block(66, 24, 8, 36, MX_WOOD);
  for(int y = 30; y < 40; y++) for(int x = 66; x < 74; x++) W.hp[IX(x, y)] = RI(3);
  flag_box(66, 30, 73, 39); W.valve_ok = 1; W.men = 3;
  for(int i = 0; i < 100; i++) mx_update(&W, DT);
  { int crumbles = 0; mx_valve_seat(&W);
    for(int f = 0; f < 60 * 6; f++){ mx_update(&W, DT); for(int i = 0; i < W.nev; i++) crumbles += W.ev[i].type == EV_CRUMBLE; if(W.man.mode == MAN_WAIT) break; }
    CHECK(W.man.mode == MAN_WAIT && crumbles == 0, "bursting on the air valve ends the shot without a crumble"); }

  printf(fails ? "crumble_test: %d FAILED\n" : "crumble_test: all passed\n", fails);
  return fails != 0;
}

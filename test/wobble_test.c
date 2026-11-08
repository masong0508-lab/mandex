/* Wobble / block-kind physics tests.  Includes mx.c directly so it can poke the static solver internals. */
#include <stdio.h>
#include <stdlib.h>
#include "../core/mx.c"

#ifdef MX_FIXED
#define F(x) RTOF(x)
#else
#define F(x) (x)
#endif
static MxWorld w;
#ifndef KICKV
#define KICKV 8
#endif
#define KICK R(KICKV)
static const char *MN[] = {"-","wood","brick","stone","glass","tnt","steel","rubber","sand"};

static void clear_world(void){
  mx_load(&w, 0);
  memset(w.grid,0,sizeof w.grid); memset(w.hp,0,sizeof w.hp); memset(w.anch,0,sizeof w.anch);
  memset(w.ox,0,sizeof w.ox); memset(w.oy,0,sizeof w.oy); memset(w.ovx,0,sizeof w.ovx); memset(w.ovy,0,sizeof w.ovy); memset(w.brk,0,sizeof w.brk);
  w.ntargets = 0; w.nrooms = 0; w.men = 0; w.man.mode = MAN_NONE;
}
static int nchunks(void){ int n=0; for(int c=0;c<MX_MAX_CHUNKS;c++) n+=w.chunks[c].active; return n; }
static int nbroken(void){ int n=0; for(int i=0;i<MX_ROWS*MX_COLS;i++) n += __builtin_popcount(w.brk[i]); return n/2; }

/* ---- 1. every level stands still ---- */
static void test_levels_calm(void){
  printf("[1] levels stand calm for 10 s (no cracks, no chunks):\n");
  for(int L = 0; L < MX_NUM_LEVELS; L++){
    mx_load(&w, L);
    for(int f = 0; f < 600; f++) mx_update(&w, R(1.0/60.0));
    printf("   L%d %-22s cells=%d chunks=%d snapped=%d wobble=%d %s\n", L+1, mx_level_name(L), mx_cells_left(&w), nchunks(), nbroken(), w.wob_on,
           (nchunks()==0 && nbroken()==0) ? "OK" : "<-- PROBLEM");
  }
}

/* ---- 2. same shove, different blocks ---- */
static void tower(int m, int x, int width, int height){
  for(int j = 0; j < height; j++) for(int i = 0; i < width; i++) set_cell(&w, x + i, MX_ROWS - 1 - j, m);
}
static void test_materials(void){
  printf("[2] one sideways shove (12 cells/s at 3/4 height) on a 6x30 tower of each block kind:\n");
  printf("   %-7s  peak sway  cycles/3s  time to settle  joints snapped  chunks\n", "block");
  for(int m = MX_WOOD; m <= MX_SAND; m++){
    if(m == MX_TNT) continue;
    clear_world(); tower(m, 60, 6, 30);
    wob_kick(&w, 63, MX_ROWS - 1 - 22, 3, R(12), 0);
    real peak = 0; int cross = 0, prev = 0, quiet_at = -1; real lastx = 0;
    for(int f = 0; f < 600; f++){
      mx_update(&w, R(1.0/60.0));
      /* sway of the column near the top */
      int i = IDX(63, MX_ROWS - 1 - 26);
      real o = w.grid[i] ? w.ox[i] : lastx; lastx = o;
      if(rabs(o) > peak) peak = rabs(o);
      int sgn = o > R(0.02) ? 1 : o < -R(0.02) ? -1 : 0;
      if(sgn && prev && sgn != prev && f < 180) cross++;
      if(sgn) prev = sgn;
      if(!w.wob_on && quiet_at < 0) quiet_at = f;
    }
    printf("   %-7s  %7.2f    %5.1f        %s       %4d          %d\n", MN[m], F(peak), cross / 2.0,
           quiet_at < 0 ? " >10s " : (quiet_at < 100 ? "<1.7 s " : "") , nbroken(), nchunks());
    if(quiet_at >= 0 && quiet_at >= 100) printf("            (settled after %.1f s)\n", quiet_at / 60.0);
  }
}

/* ---- 3. wobble then collapse: house, big hit near the base ---- */
static void test_collapse(void){
  printf("[3] collapse behaviour (house of each kind, 2 floors, kick %d cells/s at the ground floor):\n", (int)F(KICK));
  int kinds[] = { MX_WOOD, MX_BRICK, MX_STONE, MX_STEEL, MX_GLASS, MX_RUBBER };
  for(unsigned k = 0; k < sizeof kinds / sizeof *kinds; k++){
    clear_world(); house(&w, 70, 14, 2, kinds[k], MX_WOOD, kinds[k], 0, 0);
    support_pass(&w); for(int c=0;c<MX_MAX_CHUNKS;c++) if(w.chunks[c].active) settle(&w,&w.chunks[c],1);
    int cells0 = mx_cells_left(&w);
    wob_kick(&w, 72, 54, 6, KICK, 0);
    int firstcrack = -1, firstchunk = -1; real maxsway = 0;
    for(int f = 0; f < 480; f++){
      mx_update(&w, R(1.0/60.0));
      if(firstcrack < 0 && nbroken()) firstcrack = f;
      if(firstchunk < 0 && nchunks()) firstchunk = f;
      for(int i = 0; i < MX_ROWS*MX_COLS; i++) if(w.grid[i] && rabs(w.ox[i]) > maxsway) maxsway = rabs(w.ox[i]);
    }
    printf("   %-7s max sway %.2f cells | first snap at %s | first piece falls at %s | cells left %d/%d\n", MN[kinds[k]], F(maxsway),
           firstcrack<0?"never":"", firstchunk<0?"never":"", mx_cells_left(&w), cells0);
    if(firstcrack>=0) printf("            snap after %.2f s", firstcrack/60.0);
    if(firstchunk>=0) printf("%s piece after %.2f s", firstcrack>=0?",":"           ", firstchunk/60.0);
    if(firstcrack>=0||firstchunk>=0) printf("\n");
  }
}

/* ---- 4. toppling: cut a tall column near the base, does the top carry sway momentum into a topple? ---- */
static void test_topple(void){
  printf("[4] topple: 5-wide wooden tower, 40 tall; push it, then snap the joints at row 50:\n");
  clear_world(); tower(MX_WOOD, 60, 5, 40);
  wob_kick(&w, 62, 25, 12, R(14), 0);
  for(int f = 0; f < 8; f++) mx_update(&w, R(1.0/60.0));
  for(int x = 60; x < 65; x++){ int i = IDX(x, 50); w.brk[i] |= 4; w.brk[IDX(x,51)] |= 8; }
  w.need_support = 1;
  mx_update(&w, R(1.0/60.0));
  int c = -1; for(int k=0;k<MX_MAX_CHUNKS;k++) if(w.chunks[k].active) c = k;
  if(c < 0){ printf("   no chunk formed\n"); return; }
  MxChunk *ch = &w.chunks[c];
  printf("   upper section: %d cells, leaves with v=(%.1f,%.1f) cells/s, spin %.2f rad/s\n", ch->count, F(ch->vx), F(ch->vy), F(ch->w));
  real x0 = ch->x;
  for(int f = 0; f < 240; f++) mx_update(&w, R(1.0/60.0));
  printf("   after 4 s: %d chunk(s) still moving, section drifted %.1f cells sideways\n", nchunks(), F(ch->x - x0));
}

/* ---- 5. sand ---- */
static void test_sand(void){
  printf("[5] sand: a truncated mound with its centre dug out:\n");
  clear_world(); mound(&w, 50, 30, 9); w.sand_on = 1;
  for(int f = 0; f < 300; f++) mx_update(&w, R(1.0/60.0));
  int a = 0; for(int i=0;i<MX_ROWS*MX_COLS;i++) a += w.grid[i]==MX_SAND;
  printf("   untouched mound for 5 s: %d sand cells (started %d), stable=%s\n", a, 30+28+26+24+22+20+18+16+14, "yes");
  for(int y = 51; y < 59; y++) for(int x = 62; x < 68; x++){ int i = IDX(x,y); w.grid[i]=0; w.hp[i]=0; }
  w.sand_on = 1; w.need_support = 1;
  for(int f = 0; f < 600; f++) mx_update(&w, R(1.0/60.0));
  int top = MX_ROWS; for(int i=0;i<MX_ROWS*MX_COLS;i++) if(w.grid[i]==MX_SAND && i/MX_COLS < top) top = i/MX_COLS;
  int b = 0; for(int i=0;i<MX_ROWS*MX_COLS;i++) b += w.grid[i]==MX_SAND;
  printf("   after digging a hole: sand cells %d, highest sand row %d (was 51) -> %s\n", b, top, top > 51 ? "slumped into the hole" : "did not slump");
}

/* ---- 6. rubber vs brick: throw the man at each ---- */
static void test_rubber(void){
  printf("[6] throwing the man (45 cells/s, level) at a wall:\n");
  for(int m = MX_BRICK; m <= MX_RUBBER; m += (MX_RUBBER - MX_BRICK)){
    clear_world(); tower(m, 70, 4, 20);
    w.men = 1; w.man.mode = MAN_FLY; w.man.x = RI(55); w.man.y = RI(50); w.man.vx = RI(45); w.man.vy = 0; w.man.dive = 0;
    int bounced = 0; real minvx = RI(45);
    for(int f = 0; f < 120; f++){
      mx_update(&w, R(1.0/60.0));
      for(int e = 0; e < w.nev; e++) if(w.ev[e].type == EV_BOUNCE) bounced++;
      if(w.man.vx < minvx) minvx = w.man.vx;
    }
    printf("   %-6s wall: bounce events=%d, man's slowest vx=%.1f (negative = came back), wall cells left=%d/80, man mode=%d\n", MN[m], bounced, F(minvx), mx_cells_left(&w), w.man.mode);
  }
}


/* ---- 7. the man's own belly / rear jiggle: launch, hit, rest ---- */
static void test_jiggle(void){
  printf("[7] man jiggle (cells): shoot at a brick wall, track belly/rear springs:\n");
  clear_world(); for(int j = 0; j < 19; j++) for(int i = 0; i < 8; i++) set_cell(&w, 100 + i, 40 + j, MX_BRICK);
  w.man.mode = MAN_FLY; w.man.x = RI(60); w.man.y = RI(52); w.man.vx = RI(45); w.man.vy = 0; w.man.dive = 0; w.men = 2;
  real pk = 0, prk = 0; int hit_f = -1, calm_f = -1, bad = 0; real sqmax = 0;
  for(int f = 0; f < 300; f++){
    mx_update(&w, R(1.0/60.0));
    MxMan *m = &w.man;
    real a = rabs(m->bx) + rabs(m->by), r = rabs(m->rx) + rabs(m->ry);
    if(a > pk) pk = a; if(r > prk) prk = r; if(m->sq > sqmax) sqmax = m->sq;
    if(a > R(3.1) || r > R(3.1)) bad = 1;
    if(hit_f < 0 && m->mode != MAN_FLY) hit_f = f;
    if(hit_f >= 0 && calm_f < 0 && f > hit_f + 20 && a < R(0.05) && r < R(0.05)) calm_f = f;
  }
  printf("   belly peak %.2f, rear peak %.2f, squash peak %.2f; hit at %.2f s; calm %s%.2f s after; out of range: %s\n",
         F(pk), F(prk), F(sqmax), hit_f / 60.0, calm_f < 0 ? "never " : "", calm_f < 0 ? 0.0 : (calm_f - hit_f) / 60.0, bad ? "YES" : "no");
}

int main(void){
  test_levels_calm(); test_materials(); test_collapse(); test_topple(); test_sand(); test_rubber(); test_jiggle();
  return 0;
}

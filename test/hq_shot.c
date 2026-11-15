/* gcc -std=c99 -O2 -I../core ../core/mx.c hq_shot.c -lm -o hq_shot && ./hq_shot out.ppm   : the three Landlord HQ districts side by side (3 px per cell, landlords red) */
#include <stdio.h>
#include <string.h>
#include "mx.h"
static MxWorld w;
int main(int argc, char **argv){
  const int S = 3, PW = MX_COLS * S, PH = MX_ROWS * S;
  static unsigned char img[3 * 360 * 180 * 3]; memset(img, 255, sizeof img);
  mx_load(&w, MX_HQ_LEVEL);
  for(int d = 0; d < 3; d++){
    for(int y = 0; y < MX_ROWS; y++) for(int x = 0; x < MX_COLS; x++){ int m = w.grid[y * MX_COLS + x]; if(!m) continue; const unsigned char *c = mx_material_rgb(m);
      for(int j = 0; j < S; j++) for(int i = 0; i < S; i++){ unsigned char *p = img + ((y * S + j) * (3 * PW) + d * PW + x * S + i) * 3; p[0] = c[0]; p[1] = c[1]; p[2] = c[2]; } }
    for(int t = 0; t < w.ntargets; t++){ int x = (int)RTOF(w.targets[t].x), y = (int)RTOF(w.targets[t].y);
      for(int j = -3; j <= 3; j++) for(int i = -3; i <= 3; i++){ int px = x * S + i, py = y * S + j; if(px < 0 || py < 0 || px >= PW || py >= PH) continue; unsigned char *p = img + (py * (3 * PW) + d * PW + px) * 3; p[0] = 255; p[1] = 0; p[2] = 40; } }
    int sx = (int)RTOF(MX_ANCHOR_X), sy = (int)RTOF(MX_ANCHOR_Y); for(int j = 0; j < 12; j++) for(int i = -1; i <= 1; i++){ unsigned char *p = img + ((sy * S + j) * (3 * PW) + d * PW + (sx * S + i)) * 3; p[0] = 0; p[1] = 0; p[2] = 0; }
    for(int t = 0; t < w.ntargets; t++) w.targets[t].alive = 0;
    for(int f = 0; f < 200 && w.district == d && d < 2; f++) mx_update(&w, R(1.0 / 60));
  }
  FILE *o = fopen(argc > 1 ? argv[1] : "hq.ppm", "wb"); fprintf(o, "P6\n%d %d\n255\n", 3 * PW, PH); fwrite(img, 1, 3 * PW * PH * 3, o); fclose(o); return 0;
}

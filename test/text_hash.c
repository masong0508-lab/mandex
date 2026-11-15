/* Pixel-exact check for the text drawing: prints a hash per case; build once against the old gfx.c and once against the new and diff the output.
   gcc -std=gnu99 -O2 -DMX_FIXED -Istubs -I../core -I../ds/source text_hash.c <gfx.c> ../core/mx.c ../core/mx_meta.c ../core/cloth.c -lm */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gfx.h"
static u8 buf[256 * 192];
int main(void){
  static const char *S[] = { "MANDEX", "HELLO WORLD 0123456789", "A", "", "THE LANDLORDS LIVE...", "+HP", "$1,234.56 !?:-", "x" };
  unsigned seed = 7; unsigned long long all = 0;
  gfx_init();
  for(int c = 0; c < 4000; c++){
    seed = seed * 1103515245u + 12345u; int x = (int)((seed >> 8) % 340) - 50;
    seed = seed * 1103515245u + 12345u; int y = (int)((seed >> 8) % 240) - 25;
    seed = seed * 1103515245u + 12345u; const char *s = S[(seed >> 8) % 8];
    seed = seed * 1103515245u + 12345u; int sc = 1 + (seed >> 8) % 3, kind = (seed >> 12) % 3;
    memset(buf, 3, sizeof buf);
    if(kind == 0) gfx_text(buf, x, y, s, 200, sc); else if(kind == 1) gfx_text_o(buf, x, y, s, 200, 9, sc); else gfx_text_c(buf, x, y, s, 200, 9, sc);
    unsigned long long h = 1469598103934665603ULL; for(int i = 0; i < 256 * 192; i++){ h ^= buf[i]; h *= 1099511628211ULL; }
    all = all * 31 + h;
  }
  printf("%llx\n", all); return 0;
}

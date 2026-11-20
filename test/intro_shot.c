/* Contact sheets of the shared cutscenes:  ./intro_shot out.ppm  W S [T ...]   (W = 0 origin story / 1 wolfman / 2 chapter 1 ending / 3 chapter 2 ending, S = scene, T = ticks into the scene; several T make a row)
   build: gcc -std=gnu99 -O1 -DMX_SHARED_CUTSCENE -Dgfx_init=ds_gfx_init -I../dreamcast/shim -Istubs -I../core -I../ds/source intro_shot.c dc_gfx.o dc_intro.o -lm -o intro_shot */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nds.h"
#include "intro.h"
extern unsigned short gfx_pal[256];
void ds_gfx_init(void);
void sfx_play(int id, int pan){ (void)id; (void)pan; }
static unsigned char top[256 * 192], bot[256 * 192];
int main(int argc, char **argv){
  if(argc < 5 || argc > 9) return 1;  /* max five stills in the fixed preview buffer */
  int w = atoi(argv[2]), s = atoi(argv[3]), n = argc - 4;
  ds_gfx_init(); if(w == 1) intro_start_wolf(); else if(w == 2 || w == 3) intro_start_end(w - 1); else intro_start();
  static unsigned char img[256 * 5 * 192 * 2 * 3]; memset(img, 0, sizeof img);
  int tk = 0, in = -1, ti[16]; for(int i = 0; i < n; i++) ti[i] = atoi(argv[4 + i]);
  for(int f = 0; f < 20000; f++){
    int done = intro_tick(0, 0, 0); if(done) break;
    if(intro_scene() == s){ if(in < 0) in = f; int t = f - in;
      for(int i = 0; i < n; i++) if(t == ti[i]){ intro_draw(top, bot);
        for(int y = 0; y < 192; y++) for(int x = 0; x < 256; x++){ unsigned short c = gfx_pal[top[y * 256 + x]]; unsigned char *p = img + ((y) * (256 * n) + i * 256 + x) * 3; p[0] = (c & 31) << 3; p[1] = ((c >> 5) & 31) << 3; p[2] = ((c >> 10) & 31) << 3; } tk++; } }
  }
  FILE *o = fopen(argv[1], "wb"); fprintf(o, "P6\n%d 192\n255\n", 256 * n); fwrite(img, 1, 256 * n * 192 * 3, o); fclose(o);
  return tk != n;
}
